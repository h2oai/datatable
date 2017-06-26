#!/usr/bin/groovy
pipeline {
  // Run pipeline in docker container defined by project-specific
  // Dockerfile.
  // Create container only on node labeled as `mr-0xc5`
  agent {
      dockerfile {
        label "mr-0xc5"
        // Enable following line after we upgrade h2o-3-dev Docker image
        // image 'opsh2oai/h2o-3-dev'
        filename "Dockerfile"
        args '-v /home/0xdiag:/data -v /home/jenkins/slave_dir_from_mr-0xb1:/data2'
        reuseNode true
      }
   }
   // Setup job options
   options {
       timeout(time: 60, unit: 'MINUTES')
       buildDiscarder(logRotator(numToKeepStr: '10'))
   }
   stages {

        stage {'Git Pull'}
         steps {
             // Checkout git repo
             checkout scm
         }
   }

        stage('Build') {
            parallel (
                "stream 1" : {
                    node ("ubuntu") {
                      sh """
                      make clean
                      make
                      """
                    }
                  },
                "stream 2" : {
                    node ("mr-0xb11") {
                      sh """
                      make clean
                      make
                      """
                    }
                }
     }
     )
}
}
