#!/usr/bin/groovy
// TOOD: rename to @Library('h2o-jenkins-pipeline-lib') _
@Library('test-shared-library') _

pipeline {
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
            parallel (
                "stream 1" : {
                  agent {
                      dockerfile {
                        label "mr-0xc8"
                        filename "Dockerfile"
                        reuseNode true
                      }

                    node {
                      stage ('Build on Linux') {
                        sh """
                        env
                        make clean
                        make
                        python setup.py bdist_wheel
                        """}
                      stage ('Test on Linux') {
                        sh """
                        python -m pytest
                        python -m pytest --cov=datatable --cov-report=html
                        """}
                    }
                  }
                },

                "stream 2" : {
                  agent {
                      dockerfile {
                        label "mr-0xb11"
                        filename "DockerfileOsx"
                        reuseNode true
                      }

                    node {
                      stage ('Build on OSX') {
                        sh """
                        env
                        make clean
                        make
                        python setup.py bdist_wheel
                        """}
                      stage ('Test on OSX') {
                        sh """
                        python -m pytest
                        python -m pytest --cov=datatable --cov-report=html
                        """}
                    }
                  }
                }
              )
            }
}
