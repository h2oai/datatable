#!/usr/bin/groovy

//------------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
//------------------------------------------------------------------------------

def buildAll(stageDir, platform, ciVersionSuffix, py36VenvCmd, py35VenvCmd, extraEnv) {
    dumpInfo()

    final def ompLogFile = "stage_build_with_omp_on_${platform}_output.log"
    final def noompLogFile = "stage_build_with_noomp_on_${platform}_output.log"

    sh "mkdir -p ${stageDir}"

    // build and archive the omp version for Python 3.6
    buildTarget(stageDir, ["CI_VERSION_SUFFIX=${ciVersionSuffix}"] + extraEnv, py36VenvCmd, 'dist', ompLogFile)
    arch "${stageDir}/dist/**/*.whl"
    // build and archive the noomp version for Python 3.6
    buildTarget(stageDir, ["CI_VERSION_SUFFIX=${ciVersionSuffix}.noomp"] + extraEnv, py36VenvCmd, 'dist_noomp', noompLogFile)
    arch "${stageDir}/dist/**/*.whl"

    // build and archive the omp version for Python 3.6
    buildTarget(stageDir, ["CI_VERSION_SUFFIX=${ciVersionSuffix}"] + extraEnv, py35VenvCmd, 'dist', ompLogFile)
    arch "${stageDir}/dist/**/*.whl"
    // build and archive the noomp version for Python 3.6
    buildTarget(stageDir, ["CI_VERSION_SUFFIX=${ciVersionSuffix}.noomp"] + extraEnv, py35VenvCmd, 'dist_noomp', noompLogFile)
    arch "${stageDir}/dist/**/*.whl"

    // build and stash the version
    buildTarget(stageDir, ["CI_VERSION_SUFFIX=${ciVersionSuffix}"] + extraEnv, py36VenvCmd, 'version', 'dist/VERSION.txt')
    stash includes: "${stageDir}/dist/VERSION.txt", name: 'VERSION'

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
        sh """
            cd ${stageDir}
            ${venvCmd}
            make coverage
        """
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
                pip install --no-cache-dir --upgrade `find dist -name "datatable-*cp\${pyVersion}*${getWheelPlatformName(platform)}*" | grep -v noomp`
                make test MODULE=datatable
            """
        }
    } finally {
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

return this

