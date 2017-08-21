#!/usr/bin/groovy
// TOOD: rename to @Library('h2o-jenkins-pipeline-lib') _
@Library('test-shared-library') _

import ai.h2o.ci.Utils
def utilsLib = new Utils()

// Paths should be absolute
def sourceDir = "/home/0xdiag"
def targetDir = "/tmp/pydatatable_large_data"

def linkMap = [ "Data" : "h2oai-benchmarks/Data",
		"smalldata" : "h2o-3/smalldata",
		"bigdata" : "h2o-3/bigdata",
		"fread" : "h2o-3/fread" ]
		   
def largeTestsRootEnv = returnIfModified("(py_)?fread\\..*", targetDir)
def dockerArgs = ""
if (!largeTestsRootEnv.isEmpty()) {
    linkFolders(sourceDir, targetDir)
    dockerArgs = makeDockerArgs(linkMap, sourceDir, targetDir)
}

pipeline {
    agent none

    // Setup job options
    options {
        ansiColor('xterm')
        timestamps()
        timeout(time: 60, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '10'))
    }


    stages {
        stage('Build on Linux') {
            agent {
                dockerfile {
                    label "docker"
                    filename "Dockerfile"
                }
            }
            steps {
                dumpInfo 'Linux Build Info'
                sh """
                        export CI_VERSION_SUFFIX=${utilsLib.getCiVersionSuffix()}
                        make mrproper
                        make build > stage_build_on_linux_output.txt
                        touch LICENSE
                        python setup.py bdist_wheel >> stage_build_on_linux_output.txt
                        python setup.py --version > dist/VERSION.txt
                """
                stash includes: 'dist/*.whl', name: 'linux_whl'
                stash includes: 'dist/VERSION.txt', name: 'VERSION'
                // Archive artifacts
                arch 'dist/*.whl'
                arch 'dist/VERSION.txt'
                arch 'stage_build_on_linux_output.txt'
            }
        }

        stage('Coverage on Linux') {
            agent {
                dockerfile {
                    label "docker"
                    filename "Dockerfile"
                    args dockerArgs
                }
            }
            steps {
                dumpInfo 'Coverage on Linux'
                sh """
                    export DT_LARGE_TESTS_ROOT="${largeTestsRootEnv}"
                    rm -rf .venv venv 2> /dev/null
                    virtualenv --python=python3.6 --no-download .venv
                    make coverage PYTHON=.venv/bin/python
                """
                testReport 'build/coverage-c', "Linux coverage report for C"
                testReport 'build/coverage-py', "Linux coverage report for Python"
            }
        }

        stage('Test on Linux') {
            agent {
                dockerfile {
                    label "docker"
                    filename "Dockerfile"
                    args "-v /tmp/pydatatable_large_data:/tmp/pydatatable_large_data -v /home/0xdiag"
                }
            }

            steps {
                unstash 'linux_whl'
                dumpInfo 'Linux Test Info'
                script {
                    try {
                        sh """
                            export DT_LARGE_TESTS_ROOT="${largeTestsRootEnv}"
                            rm -rf .venv venv 2> /dev/null
                            rm -rf datatable
                            virtualenv --python=python3.6 .venv
                            .venv/bin/python -m pip install --no-cache-dir --upgrade `find dist -name "*linux*.whl"`
                            make test PYTHON=.venv/bin/python MODULE=datatable
                        """
                    } finally {
                        junit testResults: 'build/test-reports/TEST-*.xml', keepLongStdio: true, allowEmptyResults: false
                        deleteDir()
                    }
                }
            }
        }

        stage('Build on OSX') {
            agent {
                label 'osx'
            }
            steps {
                dumpInfo 'Build Info'
                sh """
                        source /Users/jenkins/anaconda/bin/activate h2oai
                        export LLVM4=/usr/local/opt/llvm
                        export CI_VERSION_SUFFIX=${utilsLib.getCiVersionSuffix()}
                        export CI_EXTRA_COMPILE_ARGS="-DDISABLE_CLOCK_REALTIME"
                        make mrproper
                        make build
                        touch LICENSE
                        python setup.py bdist_wheel
                    """
                stash includes: 'dist/*.whl', name: 'osx_whl'
                arch 'dist/*.whl'
            }
        }

        stage('Coverage on OSX') {
            agent {
                label 'osx'
            }
            steps {
                dumpInfo 'Coverage on OSX'
                sh '''
                    export DT_LARGE_TESTS_ROOT="${largeTestsRootEnv}"
                    source /Users/jenkins/anaconda/bin/activate h2oai
                    export LLVM4=/usr/local/opt/llvm
                    export CI_EXTRA_COMPILE_ARGS="-DDISABLE_CLOCK_REALTIME"
                    env
                    PATH="$PATH:/usr/local/bin" make mrproper coverage
                '''
                testReport 'build/coverage-c', "OSX coverage report for C"
                testReport 'build/coverage-py', "OSX coverage report for Python"
            }
        }

        stage('Test on OSX') {
            agent {
                label 'osx'
            }

            steps {
                unstash 'osx_whl'
                script {
                    try {
                        sh '''
                                export DT_LARGE_TESTS_ROOT="${largeTestsRootEnv}"
                                source /Users/jenkins/anaconda/bin/activate h2oai
                                export LLVM4=/usr/local/opt/llvm
                                set +e
                                pip uninstall -y datatable 1> pip_uninstall.out 2> pip_uninstall.err
                                RC=$?
                                set -e
                                if [ $RC -eq 1 ]; then
                                    message=`cat pip_uninstall.err`
                                    if [ "$message" != "Cannot uninstall requirement datatable, not installed" ]; then
                                        echo "pip uninstall failure: $message"
                                        exit 1
                                    fi
                                fi
                                pip install --upgrade dist/*macosx*.whl
                                rm -rf datatable
                                make test MODULE=datatable
                        '''
                    } finally {
                        junit testResults: 'build/test-reports/TEST-*.xml', keepLongStdio: true, allowEmptyResults: false
                        deleteDir()
                    }
                }
            }
        }

        // Publish into S3 all snapshots versions
        stage('Publish snapshot to S3') {
            when {
                branch 'master'
            }
            agent {
                label "linux"
            }
            steps {
                unstash 'linux_whl'
                unstash 'osx_whl'
                unstash 'VERSION'
                sh 'echo "Stashed files:" && ls -l dist'
                script {
                    def versionText = utilsLib.getCommandOutput("cat dist/VERSION.txt")
                    def version = utilsLib.fragmentVersion(versionText)
                    def _majorVersion = version[0]
                    def _buildVersion = version[1]
                    version = null // This is necessary, else version:Tuple will be serialized
                    s3up {
                        localArtifact = 'dist/*.whl'
                        artifactId = "pydatatable"
                        majorVersion = _majorVersion
                        buildVersion = _buildVersion
                        keepPrivate = true
                    }
                }
            }
        }
    }
}

def returnIfModified(pattern, value) {
    node {
        checkout scm
        out = sh script: """
                            if [ \$(\
                                git diff-tree --no-commit-id --name-only -r HEAD | \
                                xargs basename | \
                                egrep -e '${pattern}' | \
                                wc -l) \
                              -gt 0 ]; then
                            echo -n "${value}"; fi
                         """, returnStdout: true
    }
    return out
}

def linkFolders(sourceDir, targetDir) {
    node {
        sh """
            mkdir ${targetDir} || true
        
            mkdir ${targetDir}/h2oai-benchmarks || true
            ln -sf ${sourceDir}/Data ${targetDir}/h2oai-benchmarks
        
            mkdir ${targetDir}/h2o-3 || true
            ln -sf ${sourceDir}/smalldata ${targetDir}/h2o-3
            ln -sf ${sourceDir}/bigdata ${targetDir}/h2o-3
            ln -sf ${sourceDir}/fread ${targetDir}/h2o-3
        """
    }
}

	       
static makeDockerArgs(linkMap, sourceDir, targetDir) {
    def out = ""
    linkMap.each { key, value ->
        out += "-v ${sourceDir}/${key}:${targetDir}/${value} "
    }
    return out
}
