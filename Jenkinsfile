#!/usr/bin/groovy
// TOOD: rename to @Library('h2o-jenkins-pipeline-lib') _
@Library('test-shared-library') _

import ai.h2o.ci.Utils
def utilsLib = new Utils()

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
                }
            }
            steps {
                dumpInfo 'Coverage on Linux'
                sh """
                    rm -rf .venv venv 2> /dev/null
                    virtualenv --python=python3.6 --no-download .venv
                    .venv/bin/python -m pip install .[testing] --upgrade --no-cache-dir
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
                }
            }
            steps {
                unstash 'linux_whl'
                dumpInfo 'Linux Test Info'
                script {
                    try {
                        sh """
                            rm -rf .venv venv 2> /dev/null
                            rm -rf datatable
                            virtualenv --python=python3.6 .venv
                            .venv/bin/python -m pip install --no-cache-dir --upgrade `find dist -name "*linux*.whl"`[testing]
                            make test PYTHON=.venv/bin/python
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
                                make test
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

