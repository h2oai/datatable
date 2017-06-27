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

        stage('Git Pull') {
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
        }

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
            }
        }
        stage('Build on OSX') {
            agent {
                label "mr-0xb11"
                reuseNode true
            }
            steps {
                checkout scm
                sh """
                        sed -i "s/python/python3.6/" Makefile
                        sed -i "s/pip/pip3.6/" Makefile 
                        env
                        make clean
                        make build
                        touch LICENSE
                        python3.6 setup.py bdist_wheel
                    """
            }
        }


    }
}

/*stage('Test on Linux') {
    steps {
        sh """
                python3.6 setup.py install
                python3.6 -m pytest
                python3.6 -m pytest --cov=datatable --cov-report=html
                """
    }
}

/*
stage('Build on OSX') {
    steps {
        sh """
                env
                make clean
                make
                python setup.py bdist_wheel
                """
    }
}
stage('Test on OSX') {
    steps {
        sh """
                python -m pytest
                python -m pytest --cov=datatable --cov-report=html
                """
    }
}
*/

// Publish into S3 all snapshots versions
/*stage('Publish snapshot to S3') {
    when {
        branch 'master'
    }
    agent {
        dockerfile {
            label "mr-0xc5"
            filename "Dockerfile"
            reuseNode true
        }
    }
    steps {
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
}*/