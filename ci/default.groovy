#!/usr/bin/groovy

//------------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
//------------------------------------------------------------------------------

def build(platform, ciVersionSuffix, pythonBin, extraEnv) {
    dumpInfo()
    withEnv(["CI_VERSION_SUFFIX=${ciVersionSuffix}"] + extraEnv) {
        sh """
                make mrproper
                make dist PYTHON=${pythonBin} > stage_build_with_omp_on_${platform}_output
                make version PYTHON=${pythonBin} > dist/VERSION.txt

        """
    }

    stash includes: 'dist/**/*.whl', name: "${platform}_omp"
    stash includes: 'dist/VERSION.txt', name: 'VERSION'
    arch 'dist/**/*.whl'

    // Create also no omp version
    withEnv(["CI_VERSION_SUFFIX=${ciVersionSuffix}.noomp"] + extraEnv) {
        sh """
                make clean
                make dist_noomp PYTHON=${pythonBin} >> stage_build_without_omp_on_${platform}_output.txt
        """
    }
    stash includes: 'dist/**/*.whl', name: "${platform}_noomp"
    arch 'dist/**/*.whl'
    // Archive logs
    arch 'stage_build_*.txt'
}

def coverage(platform, pythonBin, extraEnv, invokeLargeTests, targetDataDir) {
    dumpInfo()
    def extEnv = []
    if (invokeLargeTests) {
        extEnv += "DT_LARGE_TESTS_ROOT=${targetDataDir}"
    }

    withEnv(extEnv + extraEnv) {
        sh '''
            make mrproper
            rm -rf .venv venv 2> /dev/null
            virtualenv --python=python3.6 --no-download .venv
            make coverage PYTHON=.venv/bin/python
        '''
    }
    testReport 'build/coverage-c', "${platform} coverage report for C"
    testReport 'build/coverage-py', "${platform} coverage report for Python"
}

def test(platform, pythonBin, extraEnv, invokeLargeTests, targetDataDir) {
    dumpInfo()
    sh "make mrproper"
    unstash "${platform}_omp"
    def extEnv = invokeLargeTests ? ["DT_LARGE_TESTS_ROOT=${targetDataDir}"] : []
    try {
        withEnv(extEnv + extraEnv) {
            sh """
                rm -rf .venv venv 2> /dev/null
                rm -rf datatable
                virtualenv --python=python3.6 .venv
                .venv/bin/python -m pip install --no-cache-dir --upgrade `find dist -name "datatable-*" | grep -v noomp`
                make test PYTHON=.venv/bin/python MODULE=datatable
            """
        }
    } finally {
        junit testResults: 'build/test-reports/TEST-*.xml', keepLongStdio: true, allowEmptyResults: false
        deleteDir()
    }
}

return this

