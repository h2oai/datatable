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

        /*stage('Git Pull') {
            agent {
                dockerfile {
                    label "mr-0xc5"
                    filename "Dockerfile"
                    reuseNode true
                }
            }
            steps {
                // Checkout git repo - it is defined as part of multi-branch Jenkins job
                checkout scm
            }
        }*/

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
                        sed -i "s/python/python3.6/" Makefile
                        sed -i "s/pip/pip3.6/" Makefile 
                        env
                        make clean
                        make build
                        touch LICENSE
                        python3.6 setup.py bdist_wheel
                        """
                stash includes: '**/dist/*.whl', name: 'linux_whl'
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
            }
        }
        // Publish into S3 all snapshots versions
        stage('Publish snapshot to S3') {
            when {
                branch 'dev'
            }
            agent any
            steps {
                unstash 'linux_whl'
                unstash 'osx_whl'
                sh "ls -l dist"
                /*script {
                    def _majorVersion = "0.1" // TODO: read from file
                    def _buildVersion = "${env.BUILD_ID}"
                    s3up {
                        localArtifact = 'dist/*.whl'
                        artifactId = "pydatatable"
                        majorVersion = _majorVersion
                        buildVersion = _buildVersion
                        keepPrivate = true
                    }
                }*/
            }
        }

    }
}

