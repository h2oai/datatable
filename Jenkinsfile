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
                        export LLVM4=/usr/local/clang+llvm-4.0.1-x86_64-apple-macosx10.9.0
                        export PATH=/usr/local/bin:$PATH
                        source ../h2oai_venv/bin/activate
                        env
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
                        sh """
                                export LLVM4=/usr/local/clang+llvm-4.0.1-x86_64-apple-macosx10.9.0
                                export PATH=/usr/local/bin:$PATH
                                source ../h2oai_venv/bin/activate
                                pip install dist/*macosx*.whl
                                python -m pytest --junit-xml=build/test-reports/TEST-datatable_osx.xml
                        """
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
                    def _buildVersion = "${env.BUILD_ID}"
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

