#!/usr/bin/groovy
//------------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
//------------------------------------------------------------------------------

GENERIC_VERSION_REGEX = /^## \[(\d+\.)+\d\].*/

def buildAll(stageDir, platform, ciVersionSuffix, py36VenvCmd, py35VenvCmd, extraEnv) {
    dumpInfo()

    final def ompLogFile = "stage_build_with_omp_on_${platform}_output.log"

    sh "mkdir -p ${stageDir}"

    // build and archive the omp version for Python 3.6
    buildTarget(stageDir, ["CI_VERSION_SUFFIX=${ciVersionSuffix}"] + extraEnv, py36VenvCmd, 'dist', ompLogFile)
    arch "${stageDir}/dist/**/*.whl"

    // build and archive the omp version for Python 3.5
    buildTarget(stageDir, ["CI_VERSION_SUFFIX=${ciVersionSuffix}"] + extraEnv, py35VenvCmd, 'dist', ompLogFile)
    arch "${stageDir}/dist/**/*.whl"

    // build and stash the version
    buildTarget(stageDir, ["CI_VERSION_SUFFIX=${ciVersionSuffix}"] + extraEnv, py36VenvCmd, 'version', 'dist/VERSION.txt')
    stash includes: "${stageDir}/dist/VERSION.txt", name: 'VERSION'

    // Archive logs
    arch "${stageDir}/stage_build_*.log"
}

def buildSDist(stageDir, ciVersionSuffix, py36VenvCmd) {
    final def sDistLogFile = "stage_build_sdist_output.log"

    // build and archive the sdist
    buildTarget(stageDir, ["CI_VERSION_SUFFIX=${ciVersionSuffix}"], py36VenvCmd, 'sdist', sDistLogFile)
    arch "${stageDir}/dist/**/*.tar.gz"

    // Archive logs
    arch "${stageDir}/stage_build_*.log"
}

def buildTarget(stageDir, extEnv, venvCmd, target, logfile) {
    withEnv(extEnv) {
        sh """
            cd ${stageDir}
            ${venvCmd}
            make clean
            mkdir -p \$(dirname ${logfile})
            make ${target} >> ${logfile}
        """
    }
}

def coverage(stageDir, platform, venvCmd, extraEnv, invokeLargeTests, targetDataDir) {
    dumpInfo()
    def extEnv = []
    if (invokeLargeTests) {
        extEnv += "DT_LARGE_TESTS_ROOT=${targetDataDir}"
    }

    withEnv(extEnv + extraEnv) {
        try {
            sh """
                cd ${stageDir}
                ${venvCmd}
                make coverage
            """
        } finally {
            // Archive core dump log
            arch "/tmp/cores/*"
        }
    }
    testReport "${stageDir}/build/coverage-c", "${platform} coverage report for C"
    testReport "${stageDir}/build/coverage-py", "${platform} coverage report for Python"
}

def test(stageDir, platform, venvCmd, extraEnv, invokeLargeTests, targetDataDir) {
    dumpInfo()
    def extEnv = invokeLargeTests ? ["DT_LARGE_TESTS_ROOT=${targetDataDir}"] : []
    try {
        withEnv(extEnv + extraEnv) {
            pullFilesFromArch("**/dist/**/*${getWheelPlatformName(platform)}*.whl", "${stageDir}/dist")

            sh """
                cd ${stageDir}
                rm -rf datatable
                ${venvCmd}
                pyVersion=\$(python --version 2>&1 | egrep -o '[0-9]\\.[0-9]' | tr -d '.')
                pip install --no-cache-dir --upgrade `find dist -name "datatable-*cp\${pyVersion}*${getWheelPlatformName(platform)}*"`
                make test_install MODULE=datatable
                make test
            """
        }
    } finally {
        // Archive core dump log
        arch "/tmp/cores/*"
        junit testResults: "${stageDir}/build/test-reports/TEST-*.xml", keepLongStdio: true, allowEmptyResults: false
    }
}

def getWheelPlatformName(final platform) {
    switch (platform){
        case 'x86_64_linux':
            return 'linux_x86_64'
        case 'x86_64_centos7':
            return 'linux_x86_64'
        case 'x86_64_macos':
            return 'macosx_10_7_x86_64'
        case 'ppc64le_linux':
            return 'linux_ppc64le'
    }
}

def getS3PlatformName(final platform) {
    echo "platform ${platform}"
    switch (platform){
        case 'x86_64_linux':
            return 'x86_64-linux'
        case 'x86_64_centos7':
            return 'x86_64-centos7'
        case 'x86_64_macos':
            return 'x86_64-osx'
        case 'ppc64le_linux':
            return 'ppc64le-centos7'
    }
}

def pullFilesFromArch(final String filter, final String targetDir) {
    sh "mkdir -p ${targetDir}"
    step([$class              : 'CopyArtifact',
          projectName         : env.JOB_NAME,
          fingerprintArtifacts: true,
          filter              : filter,
          selector            : [$class: 'SpecificBuildSelector', buildNumber: env.BUILD_ID],
          target              : "${targetDir}/",
          flatten             : true
    ])
}

def getChangelogPartForVersion(final version, final changelogPath = 'CHANGELOG.md') {
    if (version == null || version.trim().isEmpty()) {
        error 'Version must be set'
    }

    echo "Reading changelog from ${changelogPath}"
    def changelogLines = readFile(changelogPath).readLines()

    def startIndex = changelogLines.findIndexOf {
        it ==~ /^## \[${version}\].*/
    }
    if (startIndex == -1) {
        error 'Cannot find Changelog for this version'
    }

    def endIndex = startIndex
    if ((startIndex + 1) < changelogLines.size() - 1) {
        endIndex = startIndex + changelogLines[(startIndex + 1)..-1].findIndexOf {
            it ==~ GENERIC_VERSION_REGEX
        }
    }

    return changelogLines[startIndex..endIndex].join('\n').trim()
}

def getReleaseDownloadLinksText(final folder, final s3PathPrefix) {
    def files = sh(script: "cd ${folder} && find . \\( -name '*.whl' -o -name '*.tar.gz' \\) -printf '%P\n'", returnStdout: true).trim().readLines()
    def resultLines = ['## Download links ##', '']
    files.each { file ->
        resultLines += "- [${file}](${s3PathPrefix}/${file})"
    }
    return resultLines.join('\n').trim()

}

return this

