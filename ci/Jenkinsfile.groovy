#!/usr/bin/groovy
//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------

@Library('test-shared-library@1.21') _

import ai.h2o.ci.Utils
import ai.h2o.ci.buildsummary.StagesSummary
import ai.h2o.ci.buildsummary.DetailsSummary
import ai.h2o.ci.BuildResult
import static org.jenkinsci.plugins.pipeline.modeldefinition.Utils.markStageWithTag


// initialize build summary
buildSummary('https://github.com/h2oai/datatable', true)
// use default StagesSummary implementation
buildSummary.get().addStagesSummary(this, new StagesSummary())



//------------------------------------------------------------------------------
// Global constants
//------------------------------------------------------------------------------

NODE_LINUX   = "docker && linux && !micro"
NODE_MACOS   = 'osx'
NODE_PPC     = 'ibm-power'
NODE_RELEASE = 'docker && linux && !micro'

// Paths for S3 artifacts
S3_BASE = "s3://h2o-release/datatable"
S3_URL = "https://h2o-release.s3.amazonaws.com/datatable"

// Paths should be absolute
S3_URL_STABLE = "s3://h2o-release/datatable/stable"

// Data map for linking into container
LINK_MAP = [
    "Data"     : "h2oai-benchmarks/Data",
    "smalldata": "h2o-3/smalldata",
    "bigdata"  : "h2o-3/bigdata",
    "fread"    : "h2o-3/fread",
]


DOCKER_IMAGE_PPC64LE_MANYLINUX = "quay.io/pypa/manylinux2014_ppc64le"
DOCKER_IMAGE_X86_64_MANYLINUX = "quay.io/pypa/manylinux2010_x86_64"


// Note: global variables must be declared without `def`
//       see https://stackoverflow.com/questions/6305910

// These variables will be initialized at the <checkout> stage
doLargeFreadTests = false
isMainJob         = false
isRelease         = false
doExtraTests      = false
doPpcTests        = false
doPpcBuild        = false
doPy38Tests       = false
doCoverage        = false
doPublish         = false

DT_RELEASE = ""
DT_BUILD_SUFFIX = ""
DT_BUILD_NUMBER = ""


properties([
    parameters([
        booleanParam(name: 'FORCE_ALL_TESTS',          defaultValue: false, description: '[BUILD] Run all tests (even for PR).'),
        booleanParam(name: 'DISABLE_ALL_TESTS',        defaultValue: false, description: '[BUILD] Disable all tests.'),
        booleanParam(name: 'DISABLE_PPC64LE_TESTS',    defaultValue: false, description: '[BUILD] Disable PPC64LE tests.'),
        booleanParam(name: 'FORCE_BUILD_PPC64LE',      defaultValue: false, description: '[BUILD] Trigger build of PPC64le artifacts.'),
        booleanParam(name: 'DISABLE_COVERAGE',         defaultValue: false, description: '[BUILD] Disable coverage.'),
        booleanParam(name: 'FORCE_S3_PUSH',            defaultValue: false, description: '[BUILD] Publish to S3 regardless of current branch.'),
        booleanParam(name: 'FORCE_LARGER_FREAD_TESTS', defaultValue: false, description: '[BUILD] Run larger fread tests.')
    ]),
    buildDiscarder(logRotator(artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '180', numToKeepStr: ''))
])



//------------------------------------------------------------------------------
// PIPELINE
//------------------------------------------------------------------------------

ansiColor('xterm') {
    timestamps {
        cancelPreviousBuilds()
        timeout(time: 400, unit: 'MINUTES') {
            // Checkout stage
            node(NODE_LINUX) {
                def stageDir = 'checkout'
                dir (stageDir) {
                    buildSummary.stageWithSummary('Checkout and Setup Env', stageDir) {
                        deleteDir()

                        sh "git clone https://github.com/h2oai/datatable.git ."

                        isMainJob = (env.CHANGE_BRANCH == null || env.CHANGE_BRANCH == '')
                        isRelease = !isMainJob && env.CHANGE_BRANCH.startsWith("rel-")
                        doPublish = isMainJob || isRelease || params.FORCE_S3_PUSH

                        if (isMainJob) {
                            // No need to do anything: already on the main branch
                        }
                        else if (isRelease) {
                            // During the release it must be an actual checkout
                            sh "git checkout ${env.CHANGE_BRANCH}"
                        }
                        else {
                            // Note: we do not explicitly checkout the branch here,
                            // because the branch may be on the forked repo
                            sh """
                                git fetch origin +refs/pull/${env.CHANGE_ID}/merge
                                git checkout -qf FETCH_HEAD
                            """
                        }

                        buildInfo(env.BRANCH_NAME, isRelease)

                        if (!params.DISABLE_ALL_TESTS) {
                            doLargeFreadTests = (params.FORCE_LARGER_FREAD_TESTS ||
                                                  isModified("src/core/(read|csv)/.*") ||
                                                  isRelease ||
                                                  params.FORCE_ALL_TESTS)
                            doExtraTests       = (isMainJob || isRelease || params.FORCE_ALL_TESTS)
                            doPpcTests         = (doExtraTests || params.FORCE_BUILD_PPC64LE) && !params.DISABLE_PPC64LE_TESTS
                            doPpcBuild         = doPpcBuild || doPpcTests || isMainJob || isRelease || params.FORCE_BUILD_PPC64LE
                            doPy38Tests        = doExtraTests
                            doCoverage         = !params.DISABLE_COVERAGE && false   // disable for now
                        }

                        println("env.BRANCH_NAME   = ${env.BRANCH_NAME}")
                        println("env.BUILD_ID      = ${env.BUILD_ID}")
                        println("env.CHANGE_BRANCH = ${env.CHANGE_BRANCH}")
                        println("env.CHANGE_ID     = ${env.CHANGE_ID}")
                        println("env.CHANGE_TARGET = ${env.CHANGE_TARGET}")
                        println("env.CHANGE_SOURCE = ${env.CHANGE_SOURCE}")
                        println("env.CHANGE_FORK   = ${env.CHANGE_FORK}")
                        println("isMainJob         = ${isMainJob}")
                        println("isRelease         = ${isRelease}")
                        println("doPublish         = ${doPublish}")
                        println("doPpcBuild        = ${doPpcBuild}")
                        println("doLargeFreadTests = ${doLargeFreadTests}")
                        println("doExtraTests      = ${doExtraTests}")
                        println("doPy38Tests       = ${doPy38Tests}")
                        println("doPpcTests        = ${doPpcTests}")
                        println("doCoverage        = ${doCoverage}")


                        if (doPpcBuild) {
                            manager.addBadge("success.gif", "PPC64LE build triggered.")
                        }
                        if (doPublish) {
                            manager.addBadge("package.gif", "Publish to S3.")
                        }

                        if (isRelease) {
                            DT_RELEASE = 'True'
                        }
                        else if (env.BRANCH_NAME == 'main') {
                            DT_BUILD_NUMBER = sh(
                              script: "git rev-list --count main",
                              returnStdout: true
                            ).trim()
                        }
                        else {
                            def BRANCH_BUILD_ID = sh(script:
                              "git rev-list --count main..",
                              returnStdout: true
                            ).trim()
                            DT_BUILD_SUFFIX = env.BRANCH_NAME.replaceAll('[^\\w]+', '') + "." + BRANCH_BUILD_ID
                        }

                        if (doLargeFreadTests) {
                            env.DT_LARGE_TESTS_ROOT = "/tmp/pydatatable_large_data"
                            manager.addBadge("warning.gif", "Large fread tests required")
                        }

                        println("DT_RELEASE      = ${DT_RELEASE}")
                        println("DT_BUILD_NUMBER = ${DT_BUILD_NUMBER}")
                        println("DT_BUILD_SUFFIX = ${DT_BUILD_SUFFIX}")
                    }
                    buildSummary.stageWithSummary('Generate sdist & version file', stageDir) {
                        sh """
                            docker run --rm \
                                -v `pwd`:/dot \
                                -u `id -u`:`id -g` \
                                -w /dot \
                                -e DT_RELEASE=${DT_RELEASE} \
                                -e DT_BUILD_SUFFIX=${DT_BUILD_SUFFIX} \
                                -e DT_BUILD_NUMBER=${DT_BUILD_NUMBER} \
                                --entrypoint /bin/bash \
                                ${DOCKER_IMAGE_X86_64_MANYLINUX} \
                                -c "env && /opt/python/cp37-cp37m/bin/python3.7 ci/ext.py sdist"
                        """
                        sh """
                            echo "--------- _build_info.py --------------------"
                            cat src/datatable/_build_info.py
                            echo "---------------------------------------------"
                        """
                        arch "src/datatable/_build_info.py"
                        stash includes: 'src/datatable/_build_info.py', name: 'build_info'
                        stash includes: 'dist/*.tar.gz', name: 'sdist'
                        arch "dist/*.tar.gz"
                        sh "rm -f dist/*.tar.gz"
                    }
                    stash name: 'datatable-sources', useDefaultExcludes: false
                }
            }
            // Build stages
            parallel([
                'Build on x86_64-manylinux': {
                    node(NODE_LINUX) {
                        final stageDir = 'build-x86_64-manylinux'
                        buildSummary.stageWithSummary('Build on x86_64-manylinux', stageDir) {
                            cleanWs()
                            dumpInfo()
                            dir(stageDir) {
                                unstash 'datatable-sources'
                                sh """
                                    docker run --rm --init \
                                        -u `id -u`:`id -g` \
                                        -v `pwd`:/dot \
                                        -e DT_RELEASE=${DT_RELEASE} \
                                        -e DT_BUILD_SUFFIX=${DT_BUILD_SUFFIX} \
                                        -e DT_BUILD_NUMBER=${DT_BUILD_NUMBER} \
                                        --entrypoint /bin/bash \
                                        ${DOCKER_IMAGE_X86_64_MANYLINUX} \
                                        -c "cd /dot && \
                                            ls -la && \
                                            ls -la src/datatable && \
                                            /opt/python/cp37-cp37m/bin/python3.7 ci/ext.py debugwheel --audit && \
                                            /opt/python/cp37-cp37m/bin/python3.7 ci/ext.py wheel --audit && \
                                            /opt/python/cp38-cp38/bin/python3.8 ci/ext.py wheel --audit && \
                                            /opt/python/cp39-cp39/bin/python3.9 ci/ext.py wheel --audit && \
                                            /opt/python/cp310-cp310/bin/python3.10 ci/ext.py wheel --audit && \
                                            echo '===== Py3.7 Debug =====' && unzip -p dist/*debug*.whl datatable/_build_info.py && \
                                            mv dist/*debug*.whl . && \
                                            echo '===== Py3.7 =====' && unzip -p dist/*cp37*.whl datatable/_build_info.py && \
                                            echo '===== Py3.8 =====' && unzip -p dist/*cp38*.whl datatable/_build_info.py && \
                                            echo '===== Py3.9 =====' && unzip -p dist/*cp39*.whl datatable/_build_info.py && \
                                            echo '===== Py3.10 =====' && unzip -p dist/*cp310*.whl datatable/_build_info.py && \
                                            mv *debug*.whl dist/ && \
                                            ls -la dist"
                                """
                                stash name: 'x86_64-manylinux-debugwheels', includes: "dist/*debug*.whl"
                                stash name: 'x86_64-manylinux-wheels', includes: "dist/*.whl", excludes: "dist/*debug*.whl"
                                arch "dist/*.whl"
                            }
                        }
                    }
                },
                'Build on x86_64-macos': {
                    node(NODE_MACOS) {
                        def stageDir = 'build-x86_64-macos'
                        buildSummary.stageWithSummary('Build on x86_64-macos', stageDir) {
                            cleanWs()
                            dumpInfo()
                            dir(stageDir) {
                                unstash 'datatable-sources'
                                withEnv(["DT_RELEASE=${DT_RELEASE}",
                                         "DT_BUILD_SUFFIX=${DT_BUILD_SUFFIX}",
                                         "DT_BUILD_NUMBER=${DT_BUILD_NUMBER}"]) {
                                    sh """
                                        source /Users/jenkins/datatable_envs/py37/bin/activate
                                        python ci/ext.py wheel
                                        deactivate
                                        source /Users/jenkins/datatable_envs/py38/bin/activate
                                        python ci/ext.py wheel
                                        deactivate
                                        source /Users/jenkins/datatable_envs/py39/bin/activate
                                        python ci/ext.py wheel
                                        deactivate
                                        source /Users/jenkins/datatable_envs/py310/bin/activate
                                        python ci/ext.py wheel
                                        deactivate
                                        echo '===== Py3.7 =====' && unzip -p dist/*cp37*.whl datatable/_build_info.py
                                        echo '===== Py3.8 =====' && unzip -p dist/*cp38*.whl datatable/_build_info.py
                                        echo '===== Py3.9 =====' && unzip -p dist/*cp39*.whl datatable/_build_info.py
                                        echo '===== Py3.10 =====' && unzip -p dist/*cp310*.whl datatable/_build_info.py
                                        ls dist
                                    """
                                    stash name: 'x86_64-macos-wheels', includes: "dist/*.whl"
                                    arch "dist/*.whl"
                                }
                            }
                        }
                    }
                },
                'Build on ppc64le-manylinux': {
                    if (doPpcBuild) {
                        node(NODE_PPC) {
                            final stageDir = 'build-ppc64le_centos7'
                            buildSummary.stageWithSummary('Build on ppc64le_centos7', stageDir) {
                                cleanWs()
                                dumpInfo()

                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    sh """
                                        docker run \
                                            -u `id -u`:`id -g` \
                                            -e USER=$USER \
                                            -v /etc/passwd:/etc/passwd:ro \
                                            -v /etc/group:/etc/group:ro \
                                            --rm --init \
                                            -v `pwd`:/dot \
                                            -e DT_RELEASE=${DT_RELEASE} \
                                            -e DT_BUILD_SUFFIX=${DT_BUILD_SUFFIX} \
                                            -e DT_BUILD_NUMBER=${DT_BUILD_NUMBER} \
                                            --entrypoint /bin/bash \
                                            ${DOCKER_IMAGE_PPC64LE_MANYLINUX} \
                                            -c "cd /dot && \
                                                ls -la && \
                                                ls -la src/datatable && \
                                                /opt/python/cp37-cp37m/bin/python3.7 ci/ext.py debugwheel --audit && \
                                                /opt/python/cp37-cp37m/bin/python3.7 ci/ext.py wheel --audit && \
                                                /opt/python/cp38-cp38/bin/python3.8 ci/ext.py wheel --audit && \
                                                /opt/python/cp39-cp39/bin/python3.9 ci/ext.py wheel --audit && \
                                                /opt/python/cp310-cp310/bin/python3.10 ci/ext.py wheel --audit && \
                                                echo '===== Py3.7 Debug =====' && unzip -p dist/*debug*.whl datatable/_build_info.py && \
                                                mv dist/*debug*.whl . && \
                                                echo '===== Py3.7 =====' && unzip -p dist/*cp37*.whl datatable/_build_info.py && \
                                                echo '===== Py3.8 =====' && unzip -p dist/*cp38*.whl datatable/_build_info.py && \
                                                echo '===== Py3.9 =====' && unzip -p dist/*cp39*.whl datatable/_build_info.py && \
                                                echo '===== Py3.10 =====' && unzip -p dist/*cp310*.whl datatable/_build_info.py && \
                                                mv *debug*.whl dist/ && \
                                                ls -la dist"
                                    """
                                    stash name: 'ppc64le-manylinux-debugwheels', includes: "dist/*debug*.whl"
                                    stash name: 'ppc64le-manylinux-wheels', includes: "dist/*.whl", excludes: "dist/*debug*.whl"
                                    arch "dist/*.whl"
                                }
                            }
                        }
                    } else {
                        println('Build on ppc64le-manylinux SKIPPED')
                        markSkipped("Build on ppc64le-manylinux")
                    }
                }
            ])
            // Test stages
            if (!params.DISABLE_ALL_TESTS) {
                def testStages = [:]
                testStages <<
                    namedStage('Test x86_64-manylinux-py37-debug', { stageName, stageDir ->
                        node(NODE_LINUX) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-debugwheels'
                                    test_in_docker("x86_64-manylinux-py37-debug", "37",
                                                   DOCKER_IMAGE_X86_64_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-manylinux-py37', { stageName, stageDir ->
                        node(NODE_LINUX) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-wheels'
                                    test_in_docker("x86_64-manylinux-py37", "37",
                                                   DOCKER_IMAGE_X86_64_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-manylinux-py38', doPy38Tests, { stageName, stageDir ->
                        node(NODE_LINUX) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-wheels'
                                    test_in_docker("x86_64-manylinux-py38", "38",
                                                   DOCKER_IMAGE_X86_64_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-manylinux-py39', doPy38Tests, { stageName, stageDir ->
                        node(NODE_LINUX) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-wheels'
                                    test_in_docker("x86_64-manylinux-py39", "39",
                                                   DOCKER_IMAGE_X86_64_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-manylinux-py310', doPy38Tests, { stageName, stageDir ->
                        node(NODE_LINUX) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-wheels'
                                    test_in_docker("x86_64-manylinux-py310", "310",
                                                   DOCKER_IMAGE_X86_64_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-manylinux-py37-debug', doPpcTests, { stageName, stageDir ->
                        node(NODE_PPC) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le-manylinux-debugwheels'
                                    test_in_docker("ppc64le-manylinux-py37-debug", "37",
                                                   DOCKER_IMAGE_PPC64LE_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-manylinux-py37', doPpcTests, { stageName, stageDir ->
                        node(NODE_PPC) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le-manylinux-wheels'
                                    test_in_docker("ppc64le-manylinux-py37", "37",
                                                   DOCKER_IMAGE_PPC64LE_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-manylinux-py38', doPpcTests && doPy38Tests, { stageName, stageDir ->
                        node(NODE_PPC) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le-manylinux-wheels'
                                    test_in_docker("ppc64le-manylinux-py38", "38",
                                                   DOCKER_IMAGE_PPC64LE_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-manylinux-py39', doPpcTests && doPy38Tests, { stageName, stageDir ->
                        node(NODE_PPC) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le-manylinux-wheels'
                                    test_in_docker("ppc64le-manylinux-py39", "39",
                                                   DOCKER_IMAGE_PPC64LE_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-manylinux-py310', doPpcTests && doPy38Tests, { stageName, stageDir ->
                        node(NODE_PPC) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le-manylinux-wheels'
                                    test_in_docker("ppc64le-manylinux-py310", "310",
                                                   DOCKER_IMAGE_PPC64LE_MANYLINUX)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-macos-py37', { stageName, stageDir ->
                        node(NODE_MACOS) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-macos-wheels'
                                    test_macos('37')
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-macos-py38', doPy38Tests, { stageName, stageDir ->
                        node(NODE_MACOS) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-macos-wheels'
                                    test_macos('38')
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-macos-py39', doPy38Tests, { stageName, stageDir ->
                        node(NODE_MACOS) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-macos-wheels'
                                    test_macos('39')
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-macos-py310', { stageName, stageDir ->
                        node(NODE_MACOS) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-macos-wheels'
                                    test_macos('310')
                                }
                            }
                        }
                    })
                // Execute defined stages in parallel
                parallel(testStages)
            }
            // Coverage stages
            if (doCoverage) {
                parallel ([
                    'Coverage on x86_64_linux': {
                        node(NODE_LINUX) {
                            final stageDir = 'coverage-x86_64_linux'
                            buildSummary.stageWithSummary('Coverage on x86_64_linux', stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    try {
                                        sh "make ubuntu_coverage_py36_with_pandas_in_docker"
                                    } finally {
                                        arch "/tmp/cores/*python*"
                                    }
                                    testReport "build/coverage-c", "x86_64_linux coverage report for C"
                                    testReport "build/coverage-py", "x86_64_linux coverage report for Python"
                                }
                            }
                        }
                    },
                    'Coverage on x86_64_macos': {
                        node(NODE_MACOS) {
                            final stageDir = 'coverage-x86_64_osx'
                            buildSummary.stageWithSummary('Coverage on x86_64_macos', stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    sh """
                                        source /Users/jenkins/datatable_envs/py310/bin/activate
                                        make coverage
                                    """
                                    testReport "build/coverage-c", "x86_64_osx coverage report for C"
                                    testReport "build/coverage-py", "x86_64_osx coverage report for Python"
                                }
                            }
                        }
                    }
                ])
            }
            // Publish snapshot to S3
            if (doPublish) {
                node(NODE_RELEASE) {
                    def stageDir = 'publish-snapshot'
                    buildSummary.stageWithSummary('Publish Snapshot to S3', stageDir) {
                        cleanWs()
                        dumpInfo()
                        dir(stageDir) {
                            sh "rm -rf dist"
                            unstash 'x86_64-manylinux-wheels'
                            unstash 'x86_64-manylinux-debugwheels'
                            unstash 'x86_64-macos-wheels'
                            unstash 'sdist'
                            unstash 'build_info'
                            if (doPpcBuild) {
                                unstash 'ppc64le-manylinux-wheels'
                                unstash 'ppc64le-manylinux-debugwheels'
                            }
                            sh "ls dist/"

                            def versionText = sh(script: """sed -ne "s/.*version='\\([^']*\\)',/\\1/p" src/datatable/_build_info.py""", returnStdout: true).trim()
                            println("versionText = ${versionText}")

                            def s3cmd = ""
                            def pyindex_links = ""
                            dir("dist") {
                                findFiles(glob: "*").each {
                                    def sha256 = sh(script: """python -c "import hashlib;print(hashlib.sha256(open('${it.name}', 'rb').read()).hexdigest())" """,
                                                    returnStdout: true).trim()
                                    def s3path = "dev/datatable-${versionText}/${it.name}"
                                    s3cmd += "s3cmd put -P dist/${it.name} ${S3_BASE}/${s3path}\n"
                                    pyindex_links += """  <li><a href="${S3_URL}/${encodeURL(s3path)}#sha256=${sha256}">${it.name}</a></li>\n"""
                                }
                            }
                            println("Adding links to index.html:\n${pyindex_links}")

                            def pyindex_content = "${S3_URL}/index.html".toURL().text
                            pyindex_content -= "</body></html>\n"
                            pyindex_content += "<h2>${versionText}</h2>\n"
                            pyindex_content += "<ul>\n"
                            pyindex_content += pyindex_links
                            pyindex_content += "</ul>\n\n"
                            pyindex_content += "</body></html>\n"
                            writeFile(file: "index.html", text: pyindex_content)
                            s3cmd += "s3cmd put -P index.html ${S3_BASE}/index.html"

                            docker.withRegistry("https://harbor.h2o.ai", "harbor.h2o.ai") {
                                withCredentials([[$class: 'AmazonWebServicesCredentialsBinding', credentialsId: "awsArtifactsUploader"]]) {
                                    docker.image("harbor.h2o.ai/library/s3cmd").inside {
                                        sh s3cmd
                                    }
                                }
                            }

                            // TODO: remove?
                            s3upDocker {
                                localArtifact = 'dist/*'
                                artifactId = 'datatable'
                                version = versionText
                                keepPrivate = false
                                isRelease = isRelease
                            }
                       }
                    }
                }
            }
            // Release to S3 and GitHub
            /*
            if (isRelease) {
                // TODO: merge this stage with the previous one?
                //       The functionality is pretty much duplicated here...
                //
                node(NODE_RELEASE) {
                    def stageDir = 'release'
                    buildSummary.stageWithSummary('Release', stageDir) {
                        timeout(unit: 'HOURS', time: 24) {
                            input('Promote build?')
                        }
                        cleanWs()
                        dumpInfo()
                        dir(stageDir) {
                            checkout scm
                            unstash 'x86_64-manylinux-wheels'
                            unstash 'x86_64-manylinux-debugwheels'
                            unstash 'x86_64-macos-wheels'
                            unstash 'ppc64le-manylinux-wheels'
                            unstash 'ppc64le-manylinux-debugwheels'
                            unstash 'sdist'
                        }
                        docker.withRegistry("https://harbor.h2o.ai", "harbor.h2o.ai") {
                            withCredentials([[$class: 'AmazonWebServicesCredentialsBinding', credentialsId: "awsArtifactsUploader"]]) {
                                docker.image("harbor.h2o.ai/library/s3cmd").inside {
                                    sh """
                                        s3cmd put -P release/dist/*.whl ${S3_URL_STABLE}/datatable-${versionText}/
                                        s3cmd put -P release/dist/*.tar.gz ${S3_URL_STABLE}/datatable-${versionText}/
                                    """
                                }
                            }
                        }
                    }
                }
            }
            */
        }
    }
}



// Run datatable test suite in docker
//
// Parameters
// ----------
// testtag
//     Arbitrary string describing this test run. This variable will be
//     used as a prefix for the test-report file name.
//
// pyver
//     python version string, such as "37" or "310"
//
// docker_image
//     Name of the docker container where the tests will be run
//
def test_in_docker(String testtag, String pyver, String docker_image) {
    sh """
        rm -rf build/test-reports
        rm -rf build/cores
        mkdir -p build/test-reports
        mkdir -p build/cores
    """
    try {
        def docker_args = ""
        docker_args += "--rm --init "
        docker_args += "--ulimit core=-1 "
        docker_args += "--entrypoint /bin/bash "
        docker_args += "-u `id -u`:`id -g` "
        docker_args += "-w /dt "
        docker_args += "-e HOME=/tmp "  // this dir has write permission for all users
        docker_args += "-v `pwd`:/dt "
        docker_args += "-v `pwd`/build/cores:/tmp/cores "
        if (doLargeFreadTests) {
            LINK_MAP.each { key, value ->
                docker_args += "-v /home/0xdiag/${key}:/data/${value} "
            }
            docker_args += "-e DT_LARGE_TESTS_ROOT=/data "
        }
        docker_args += "-e DT_HARNESS=Jenkins "
        def python = get_python_for_docker(pyver, docker_image)
        def docker_cmd = ""
        docker_cmd += "cd /dt && ls dist/ && "
        docker_cmd += python + " -m pip install virtualenv --user && "
        docker_cmd += python + " -m virtualenv /tmp/pyenv && "
        docker_cmd += "source /tmp/pyenv/bin/activate && "
        docker_cmd += "python -VV && "
        docker_cmd += "pip install --upgrade pip && "
        docker_cmd += "pip install dist/datatable-*-cp" + pyver + "-*.whl && "
        docker_cmd += "pip install -r requirements_tests.txt && "
        if (docker_image == DOCKER_IMAGE_PPC64LE_MANYLINUX) {
            // On a PPC machine use our own repository which contains pre-built
            // binary wheels for pandas & numpy.
            docker_cmd += "pip install -i https://h2oai.github.io/py-repo/ "
            docker_cmd += "--extra-index-url https://pypi.org/simple/ "
            docker_cmd += "--prefer-binary "
            docker_cmd += "numpy pandas 'xlrd<=1.2.0' && "
        } else {
            docker_cmd += "pip install -r requirements_extra.txt && "
        }
        docker_cmd += "pip freeze && "
        docker_cmd += "python -c 'import datatable; print(datatable.__file__)' && "
        docker_cmd += "python -m pytest -ra --maxfail=10 -Werror -vv -s --showlocals " +
                            " --junit-prefix=" + testtag +
                            " --junitxml=build/test-reports/TEST-datatable.xml" +
                            " tests"
        sh """
            docker run ${docker_args} ${docker_image} -c "${docker_cmd}"
        """
    } finally {
        sh "ls -la build/cores"
        archiveArtifacts artifacts: "build/cores/*", allowEmptyArchive: true
    }
    junit testResults: "build/test-reports/TEST-*.xml", keepLongStdio: true, allowEmptyResults: false
}


def get_python_for_docker(String pyver, String image) {
    if (image == DOCKER_IMAGE_X86_64_MANYLINUX || image == DOCKER_IMAGE_PPC64LE_MANYLINUX) {
        if (pyver == "37") return "/opt/python/cp37-cp37m/bin/python3.7"
        if (pyver == "38") return "/opt/python/cp38-cp38/bin/python3.8"
        if (pyver == "39") return "/opt/python/cp39-cp39/bin/python3.9"
        if (pyver == "310") return "/opt/python/cp310-cp310/bin/python3.10"
    }
    throw new Exception("Unknown python ${pyver} for docker ${image}")
}


def test_macos(String pyver) {
    try {
        sh """
            mkdir -p /tmp/cores
            rm -f /tmp/cores/*
            env
            source /Users/jenkins/datatable_envs/py${pyver}/bin/activate
            pip install --upgrade pip
            pip install dist/datatable-*-cp${pyver}-*.whl
            pip install -r requirements_tests.txt
            pip install -r requirements_extra.txt
            pip freeze
            python -c 'import datatable; print(datatable.__file__)'
            DT_HARNESS=Jenkins \
            python -m pytest -ra --maxfail=10 -Werror -vv -s --showlocals \
                --junit-prefix=x86_64-macos-py${pyver} \
                --junitxml=build/test-reports/TEST-datatable.xml \
                tests
        """
    } finally {
        sh "ls -la /tmp/cores"
        archiveArtifacts artifacts: "/tmp/cores/*", allowEmptyArchive: true
    }
    junit testResults: "build/test-reports/TEST-datatable.xml", keepLongStdio: true, allowEmptyResults: false
}



//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

def isModified(pattern) {
    def fList
    if (isMainJob) {
        fList = buildInfo.get().getChangedFiles().join('\n')
    } else {
        sh "git fetch --no-tags --progress https://github.com/h2oai/datatable +refs/heads/${env.CHANGE_TARGET}:refs/remotes/origin/${env.CHANGE_TARGET}"
        final String mergeBaseSHA = sh(script: "git merge-base HEAD origin/${env.CHANGE_TARGET}", returnStdout: true).trim()
        fList = sh(script: "git diff --name-only ${mergeBaseSHA}", returnStdout: true).trim()
    }

    def out = ""
    if (!fList.isEmpty()) {
        out = sh(script: "echo '${fList}' | egrep -e '${pattern}' | wc -l", returnStdout: true).trim()
    }

    return !(out.isEmpty() || out == "0")
}

def markSkipped(String stageName) {
    stage (stageName) {
        println("Stage ${stageName} SKIPPED!")
        markStageWithTag(STAGE_NAME, "STAGE_STATUS", "SKIPPED_FOR_CONDITIONAL")
    }
}

def namedStage(String stageName, boolean doRun, Closure body) {
    return [ (stageName) : {
        if (doRun) {
            def stageDir = stageName.toLowerCase().replaceAll(" ", "_")
            body(stageName, stageDir)
        } else {
            markSkipped(stageName)
        }
    }]
}

def namedStage(String stageName, Closure body) {
    return namedStage(stageName, true, body)
}

def encodeURL(String str) {
   return java.net.URLEncoder.encode(str, "UTF-8")
}
