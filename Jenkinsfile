#!/usr/bin/groovy
// TOOD: rename to @Library('h2o-jenkins-pipeline-lib') _
@Library('h2o-jenkins-pipeline-lib') _

pipeline {
   // Setup job options
   options {
       ansiColor('xterm')
       timestamps()
       timeout(time: 60, unit: 'MINUTES')
       buildDiscarder(logRotator(numToKeepStr: '10'))
   }
            parallel (
                "stream 1" : {
                  agent {
                      dockerfile {
                        label "mr-0xc8"
                        filename "Dockerfile"
                        args '-v /home/0xdiag:/data -v /home/jenkins/slave_dir_from_mr-0xb1:/data2'
                        reuseNode true
                      }

                    node {
                      sh """
                      env
                      export JAVA_HOME='/usr/lib/jvm/java-8-oracle'
                      make clean
                      make
                      python setup.py bdist_wheel
                      """
                      stage {'Test on Linux'}
                    }
                  }
                },

                "stream 2" : {
                  agent {
                      dockerfile {
                        label "mr-0xb11"
                        filename "Dockerfile"
                        //args '-v /home/0xdiag:/data -v /User/jenkins/slave_dir_from_mr-0xb1:/data2'
                        reuseNode true
                      }

                    node {
                      sh """
                      env
                      make clean
                      make
                      python setup.py bdist_wheel
                      """
                      stage {'Test on Linux'}
                    }
                  }
                }
