#!/usr/bin/groovy
//------------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
//------------------------------------------------------------------------------
@Library('test-shared-library') _

import ai.h2o.ci.Utils
def utilsLib = new Utils()

// Default mapping
def platformDefaults = [
        env      : [],
        pythonBin: 'python',
        enabled  : true,
        build    : 'ci/default.groovy',
        test     : 'ci/default.groovy',
        coverage : 'ci/default.groovy',
    ]

// Build platforms parameters
def BUILD_MATRIX = [
    // Linux build definition
    x86_64_linux : platformDefaults,
    // OSX
    x86_64_macos : platformDefaults + [
        env      : ["LLVM4=/usr/local/opt/llvm", "CI_EXTRA_COMPILE_ARGS=-DDISABLE_CLOCK_REALTIME"],
        pythonBin: '/Users/jenkins/anaconda/envs/h2oai/bin/python',
    ],
    // PowerPC 64bit little-endian
    //ppc64le_linux : platformDefaults
]

// Computed version suffix
def CI_VERSION_SUFFIX = utilsLib.getCiVersionSuffix()

// Global project build trigger filled in init stage
def project

// Needs invocation of larger tests
def needsLargerTest

// Paths should be absolute
def sourceDir = "/home/0xdiag"
def targetDir = "/tmp/pydatatable_large_data"

// Data map for linking into container
def linkMap = [
        "Data"      : "h2oai-benchmarks/Data",
        "smalldata" : "h2o-3/smalldata",
        "bigdata"   : "h2o-3/bigdata",
        "fread"     : "h2o-3/fread",
    ]

def dockerArgs = createDockerArgs(linkMap, sourceDir, targetDir)

pipeline {
    agent none

    // Setup job options
    options {
        ansiColor('xterm')
        timestamps()
        timeout(time: 60, unit: 'MINUTES')
        buildDiscarder(logRotator(daysToKeepStr: '30'))
    }


    stages {
        stage('Init') {
            agent { label 'linux' }
            steps {
                script {
                    buildInfo(env.BRANCH_NAME, false)
                    project = load 'ci/default.groovy'
                    needsLargerTest = isModified("(py_)?fread\\..*|__version__\\.py")
                    if (needsLargerTest) {
                        manager.addBadge("warning.gif", "Large tests required")
                    }
                }
            }
        }
        stage('Build') {
            parallel {
                stage('Build on Linux') {
                    agent {
                        dockerfile {
                            label "docker && linux"
                            filename "Dockerfile.build"
                        }
                    }
                    steps {
                        script {
                            def p = 'x86_64_linux'
                            project.build(p, CI_VERSION_SUFFIX, BUILD_MATRIX[p]['pythonBin'], BUILD_MATRIX[p]['env'])
                        }
                    }
                }
                stage('Build on OSX') {
                    agent {
                        label 'osx'
                    }
                    steps {
                        script {
                            def p = 'x86_64_macos'
                            project.build(p, CI_VERSION_SUFFIX, BUILD_MATRIX[p]['pythonBin'], BUILD_MATRIX[p]['env'])
                        }
                    }
                }
                // stage('Build on x86_64-centos7') {
                //     agent {
                //         label "docker"
                //     }
                //     steps {
                //         dumpInfo 'x86_64-centos7 Build Info'
                //         script {
                //             sh """
                //                 make mrproper_in_docker
                //                 make BRANCH_NAME=${env.BRANCH_NAME} BUILD_NUM=${env.BUILD_ID} centos7_in_docker
                //             """
                //         }
                //         stash includes: 'dist/**/*', name: 'x86_64-centos7'
                //     }
                // }
                // stage('Build on ppc64le-centos7') {
                //     agent {
                //         label "ibm-power"
                //     }
                //     steps {
                //         dumpInfo 'ppc64le-centos7 Build Info'
                //         script {
                //             sh """
                //                 make mrproper_in_docker
                //                 make BRANCH_NAME=${env.BRANCH_NAME} BUILD_NUM=${env.BUILD_ID} centos7_in_docker
                //             """
                //         }
                //         stash includes: 'dist/**/*', name: 'ppc64le-centos7'
                //     }
                // }
            }
        }

        stage("Coverage") {
            parallel {
                stage('Coverage on Linux') {
                    agent {
                        dockerfile {
                            label "docker && linux"
                            filename "Dockerfile.build"
                            args dockerArgs
                        }
                    }
                    steps {
                        script {
                            def p = 'x86_64_linux'
                            project.coverage(p, BUILD_MATRIX[p]['pythonBin'], BUILD_MATRIX[p]['env'], false, targetDir)
                        }
                    }
                }
                stage('Coverage on OSX') {
                    agent {
                        label 'osx'
                    }
                    steps {
                        script {
                            def p = 'x86_64_macos'
                            project.coverage(p, BUILD_MATRIX[p]['pythonBin'], BUILD_MATRIX[p]['env'], false, targetDir)
                        }
                    }
                }
            }
        }

        stage("Test") {
            parallel {
                stage('Test on Linux') {
                    agent {
                        dockerfile {
                            label "docker && linux"
                            filename "Dockerfile.build"
                            args dockerArgs
                        }
                    }

                    steps {
                        script {
                            def p = 'x86_64_linux'
                            project.test(p, BUILD_MATRIX[p]['pythonBin'], BUILD_MATRIX[p]['env'], needsLargerTest, targetDir)
                        }
                    }
                }
                stage('Test on OSX') {
                    agent {
                        label 'osx'
                    }

                    steps {
                        script {
                            linkFolders(sourceDir, targetDir)
                            def p = 'x86_64_macos'
                            project.test(p, BUILD_MATRIX[p]['pythonBin'], BUILD_MATRIX[p]['env'], needsLargerTest, targetDir)
                        }
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
                label "linux && docker"
            }
            steps {
                dumpInfo()

                // Clean properly before publishing into S3
                sh "make mrproper"
                script {
                    BUILD_MATRIX.each { platform, envs ->
                        unstash "${platform}_omp"
                        unstash "${platform}_noomp"
                    }
                    unstash 'VERSION'
                    sh 'echo "Stashed files:" && find dist'
                    docker.withRegistry("https://docker.h2o.ai", "docker.h2o.ai") {
                        docker.image('s3cmd').inside {
                            def versionText = utilsLib.getCommandOutput("cat dist/VERSION.txt")
                            s3up {
                                localArtifact = 'dist/*'
                                artifactId = "pydatatable"
                                version = versionText
                                keepPrivate = false
                            }
                        }
                    }
                }
            }
        }

        // stage('Publish centos7 snapshot to S3') {
        //     when {
        //         branch 'master'
        //     }
        //     agent {
        //         label "linux && docker"
        //     }
        //     steps {
        //         sh "make mrproper"
        //         unstash 'x86_64-centos7'
        //         unstash 'ppc64le-centos7'
        //         sh 'echo "Stashed files:" && find dist'
        //         script {
        //             docker.withRegistry("https://docker.h2o.ai", "docker.h2o.ai") {
        //                 docker.image('s3cmd').inside {
        //                     def versionText = utilsLib.getCommandOutput("cat dist/x86_64-centos7/VERSION.txt")
        //                     s3up {
        //                         localArtifact = 'dist/*'
        //                         artifactId = "pydatatable"
        //                         version = versionText
        //                         keepPrivate = false
        //                     }
        //                 }
        //             }
        //         }
        //     }
        // }
    }
}

def isModified(pattern) {
    def fList = buildInfo.get().getChangedFiles().join('\n')
    out = sh(script: "echo '${fList}' | xargs basename | egrep -e '${pattern}' | wc -l", returnStdout: true).trim()
    return !(out.isEmpty() || out == "0")
}


def createDockerArgs(linkMap, sourceDir, targetDir) {
    def out = ""
    linkMap.each { key, value ->
        out += "-v ${sourceDir}/${key}:${targetDir}/${value} "
    }
    return out
}

def linkFolders(sourceDir, targetDir) {
    sh """
        mkdir ${targetDir} || true

        mkdir ${targetDir}/h2oai-benchmarks || true
        ln -sf ${sourceDir}/Data ${targetDir}/h2oai-benchmarks

        mkdir ${targetDir}/h2o-3 || true
        ln -sf ${sourceDir}/smalldata ${targetDir}/h2o-3
        ln -sf ${sourceDir}/bigdata ${targetDir}/h2o-3
        ln -sf ${sourceDir}/fread ${targetDir}/h2o-3
        find ${targetDir}
    """
}

