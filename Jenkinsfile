#!/usr/bin/groovy
// TOOD: rename to @Library('h2o-jenkins-pipeline-lib') _
@Library('test-shared-library') _

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
                    label "mr-0xc5"
                    filename "Dockerfile"
                    reuseNode true
                }
            }
            steps {
                sh """
                        . /datatable_env/bin/activate
                        env
                        make clean
                        make build
                        touch LICENSE
                        python setup.py bdist_wheel
                """
                stash includes: '**/dist/*.whl', name: 'linux_whl'
                // Archive artifacts
                arch 'dist/*.whl'

            }
        }

        stage('Test on Linux') {
            agent {
                dockerfile {
                    label "mr-0xc5"
                    filename "Dockerfile"
                    reuseNode true
                }
            }
            steps {
                unstash 'linux_whl'
                script {
                    try {
                        sh """
                                . /datatable_env/bin/activate
                                pip install dist/*linux*.whl
                                python -m pytest --junit-xml=build/test-reports/TEST-datatable_linux.xml
                        """
                    } finally {
                        junit testResults: 'build/test-reports/TEST-*.xml', keepLongStdio: true, allowEmptyResults: false
                    }
                }
            }
        }

        stage('Build on OSX') {
            agent {
                label "mr-0xb11"
            }
            steps {
                sh """
                        source /Users/jenkins/anaconda/bin/activate h2oai
                        export LLVM4=/usr/local/opt/llvm
                        make clean
                        make build
                        touch LICENSE
                        python setup.py bdist_wheel
                    """
                stash includes: '**/dist/*.whl', name: 'osx_whl'
                arch 'dist/*.whl'
            }
        }

        stage('Test on OSX') {
            agent {
                label "mr-0xb11"
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
                                python -m pytest --junit-xml=build/test-reports/TEST-datatable_osx.xml
                        '''
                    } finally {
                        junit testResults: 'build/test-reports/TEST-*.xml', keepLongStdio: true, allowEmptyResults: false
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
                label "mr-0xc5"
            }
            steps {
                unstash 'linux_whl'
                unstash 'osx_whl'
                sh "ls -l dist"
                script {
                    def _majorVersion = "0.1" // TODO: read from file
                    def _buildVersion = "${env.BUILD_NUMBER}"
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

def arch(list) {
    archiveArtifacts artifacts: list, allowEmptyArchive: true
}

