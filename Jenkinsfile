#!/usr/bin/groovy
// TOOD: rename to @Library('h2o-jenkins-pipeline-lib') _
// @Library('test-shared-library') _

pipeline {
    agent {
        dockerfile {
            label "mr-0xc8"
            filename "Dockerfile"
            reuseNode true
        }
    }

    // Setup job options
    options {
        ansiColor('xterm')
        timestamps()
        timeout(time: 60, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    stages {
        stage('Git Pull') {
            steps {
                // Checkout git repo - it is defined as part of multi-branch Jenkins job
                checkout scm
            }
        }

        stage('Build on Linux') {
            steps {
                sh """
                        sed -i "s/python/python3.6/" Makefile
                        sed -i "s/pip/pip3.6/" Makefile 
                        env
                        whoami
                        cat /etc/passwd
                        make clean
                        make
                        python setup.py bdist_wheel
                        """
            }
        }

        /*stage('Test on Linux') {
            steps {
                sh """
                        make test
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

    }
}