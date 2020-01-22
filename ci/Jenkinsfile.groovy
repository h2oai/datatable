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

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                        //
// DISCLAIMER: Code in this pipeline is currently "unwinded" for better readability of non-Jenkins users. //
//                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////
@Library('test-shared-library@1.17') _

import ai.h2o.ci.Utils
import ai.h2o.ci.buildsummary.StagesSummary
import ai.h2o.ci.buildsummary.DetailsSummary
import ai.h2o.ci.BuildResult
import static org.jenkinsci.plugins.pipeline.modeldefinition.Utils.markStageWithTag

def utilsLib = new Utils()

BuildResult result = BuildResult.FAILURE

// initialize build summary
buildSummary('https://github.com/h2oai/datatable', true)
// use default StagesSummary implementation
buildSummary.get().addStagesSummary(this, new StagesSummary())

///////////////
// CONSTANTS //
///////////////
X86_64_BUILD_NODE_LABEL = "buildMachine"
NODE_LABEL = 'docker && !mr-0xc8'
OSX_NODE_LABEL = 'osx'
PPC_NODE_LABEL = 'ibm-power'
RELEASE_NODE_LABEL = 'master'
RELEASE_BRANCH_PREFIX = 'rel-'
CREDS_ID = 'h2o-ops-personal-auth-token'
GITCONFIG_CRED_ID = 'master-gitconfig'
RSA_CRED_ID = 'master-id-rsa'
X86_64_CENTOS_DOCKER_IMAGE_NAME = "harbor.h2o.ai/opsh2oai/datatable-build-x86_64_centos7"
EXPECTED_SHAS = [
    files: [
        'ci/Dockerfile-centos7.in': '0dbfd08d5857fdaa5043ffae386895d4fe524a47',
        'ci/Dockerfile-ubuntu.in': '8dbbd6afe03062befa391c20be95daf58caee4ac',
    ]
]

OSX_ENV = ["LLVM6=/usr/local/opt/llvm@6", "CI_EXTRA_COMPILE_ARGS=-DDISABLE_CLOCK_REALTIME"]
OSX_CONDA_ACTIVATE_PATH = '/Users/jenkins/anaconda/bin/activate'
// Paths should be absolute
SOURCE_DIR = "/home/0xdiag"
TARGET_DIR = "/tmp/pydatatable_large_data"
BUCKET = 'h2o-release'
STABLE_FOLDER = 'datatable/stable'
LATEST_STABLE = 'datatable/latest_stable'
S3_URL_STABLE = "s3://${BUCKET}/${STABLE_FOLDER}"
S3_URL_LATEST_STABLE = "s3://${BUCKET}/${LATEST_STABLE}"
HTTPS_URL_STABLE = "https://${BUCKET}.s3.amazonaws.com/${STABLE_FOLDER}"
// Data map for linking into container
LINK_MAP = [
    "Data"     : "h2oai-benchmarks/Data",
    "smalldata": "h2o-3/smalldata",
    "bigdata"  : "h2o-3/bigdata",
    "fread"    : "h2o-3/fread",
]


DOCKER_IMAGE_X86_64_MANYLINUX = "quay.io/pypa/manylinux2010_x86_64"
DOCKER_IMAGE_X86_64_CENTOS = "harbor.h2o.ai/opsh2oai/datatable-build-x86_64_centos7:0.8.0-master.9"
DOCKER_IMAGE_X86_64_UBUNTU = "harbor.h2o.ai/opsh2oai/datatable-build-x86_64_ubuntu:0.8.0-master.9"
DOCKER_IMAGE_PPC64LE_CENTOS = "harbor.h2o.ai/opsh2oai/datatable-build-ppc64le_centos7:0.8.0-master.9"

// Computed version suffix
// def CI_VERSION_SUFFIX = utilsLib.getCiVersionSuffix()
// Global project build trigger filled in init stage
def project
// Needs invocation of larger tests
def needsLargerTest
def dockerArgs = createDockerArgs()
// String with current version
def versionText
// String with current git revision
def gitHash

def runExtraTests = !isPrJob() || params.FORCE_ALL_TESTS_IN_PR
def runPpcTests = doPPC() && !params.DISABLE_PPC64LE_TESTS

MAKE_OPTS = "CI=1"

//////////////
// PIPELINE //
//////////////
properties([
    parameters([
        booleanParam(name: 'FORCE_BUILD_PPC64LE', defaultValue: false, description: '[BUILD] Trigger build of PPC64le artifacts.'),
        booleanParam(name: 'DISABLE_ALL_TESTS', defaultValue: false, description: '[BUILD] Disable all tests.'),
        booleanParam(name: 'DISABLE_PPC64LE_TESTS', defaultValue: false, description: '[BUILD] Disable PPC64LE tests.'),
        booleanParam(name: 'DISABLE_COVERAGE', defaultValue: false, description: '[BUILD] Disable coverage.'),
        booleanParam(name: 'FORCE_ALL_TESTS_IN_PR', defaultValue: false, description: '[BUILD] Trigger all tests even for PR.'),
        booleanParam(name: 'FORCE_S3_PUSH', defaultValue: false, description: '[BUILD] Publish to S3 regardless of current branch.')
    ]),
    buildDiscarder(logRotator(artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '180', numToKeepStr: ''))
])

ansiColor('xterm') {
    timestamps {
        if (isPrJob()) {
            cancelPreviousBuilds()
        }
        timeout(time: 120, unit: 'MINUTES') {
            // Checkout stage
            node(X86_64_BUILD_NODE_LABEL) {
                def stageDir = 'checkout'
                dir (stageDir) {
                    buildSummary.stageWithSummary('Checkout and Setup Env', stageDir) {
                        deleteDir()
                        def scmEnv = checkout scm
                        env.BRANCH_NAME = scmEnv.GIT_BRANCH.replaceAll('origin/', '').replaceAll('/', '-')

                        if (doPPC()) {
                            manager.addBadge("success.gif", "PPC64LE build triggered.")
                        }
                        if(doPublish()) {
                            manager.addBadge("package.gif", "Publish to S3.")
                        }

                        buildInfo(env.BRANCH_NAME, isRelease())
                        project = load 'ci/default.groovy'

                        if (isRelease()) {
                            env.DT_RELEASE = 'True'
                        }
                        else if (env.BRANCH_NAME != 'master' && !env.BRANCH_NAME.startsWith(RELEASE_BRANCH_PREFIX)) {
                            env.DT_BUILD_SUFFIX = env.BRANCH_NAME.replaceAll('[^\\w]+', '') + "." + env.BUILD_ID
                        }
                        else {
                            env.DT_BUILD_NUMBER = env.BUILD_ID
                        }

                        needsLargerTest = isModified("c/(read|csv)/.*")
                        if (needsLargerTest) {
                            env.DT_LARGE_TESTS_ROOT = TARGET_DIR
                            manager.addBadge("warning.gif", "Large tests required")
                        }

                        final String dockerImageTag = sh(script: "make ${MAKE_OPTS} docker_image_tag", returnStdout: true).trim()
                        docker.image("${X86_64_CENTOS_DOCKER_IMAGE_NAME}:${dockerImageTag}").inside {
                            def dockerfileSHAsString = ""
                            EXPECTED_SHAS.files.each { filename, sha ->
                                dockerfileSHAsString += "${sha}\t${filename}\n"
                            }
                            try {
                                sh """
                                    echo "${dockerfileSHAsString}" > dockerfiles.sha
                                    sha1sum -c dockerfiles.sha
                                    rm -f dockerfiles.sha
                                """
                            } catch (e) {
                                error "Dockerfiles do not have expected checksums. Please make sure, you have built the " +
                                        "new images using the Jenkins pipeline and that you have changed the required " +
                                        "fields in this pipeline."
                                throw e
                            }
                        }
                    }
                    buildSummary.stageWithSummary('Generate version file', stageDir) {
                        sh """
                            docker run --rm \
                                -v `pwd`:/dot \
                                -w /dot \
                                --entrypoint /bin/bash \
                                ${DOCKER_IMAGE_X86_64_MANYLINUX} \
                                -c "/opt/python/cp36-cp36m/bin/python3.6 ext.py geninfo --strict"
                        """
                        sh "cat src/datatable/_build_info.py"
                        arch "src/datatable/_build_info.py"
                    }
                    stash name: 'datatable-sources', useDefaultExcludes: false
                }
            }
            // Build stages
            parallel([
                'Build on x86_64-manylinux': {
                    node(X86_64_BUILD_NODE_LABEL) {
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
                                        --entrypoint /bin/bash \
                                        ${DOCKER_IMAGE_X86_64_MANYLINUX} \
                                        -c "cd /dot && \
                                            ls -la && \
                                            ls -la src/datatable && \
                                            /opt/python/cp35-cp35m/bin/python3.5 ext.py wheel --audit && \
                                            /opt/python/cp36-cp36m/bin/python3.6 ext.py wheel --audit && \
                                            /opt/python/cp37-cp37m/bin/python3.7 ext.py wheel --audit && \
                                            /opt/python/cp38-cp38/bin/python3.8 ext.py wheel --audit && \
                                            ls -la dist"
                                """
                                stash name: 'x86_64-manylinux-wheels', includes: "dist/*.whl"
                                arch "dist/*.whl"
                            }
                        }
                    }
                },
                'Build on x86_64-macos': {
                    node(OSX_NODE_LABEL) {
                        def stageDir = 'build-x86_64-macos'
                        buildSummary.stageWithSummary('Build on x86_64-macos', stageDir) {
                            cleanWs()
                            dumpInfo()
                            dir(stageDir) {
                                unstash 'datatable-sources'
                                withEnv(OSX_ENV) {
                                    // unstash 'VERSION'
                                    // unstash 'GIT_HASH_FILE'
                                    sh """
                                        . ${OSX_CONDA_ACTIVATE_PATH} datatable-py37-with-pandas
                                        make ${MAKE_OPTS} clean
                                        make ${MAKE_OPTS} BRANCH_NAME=${env.BRANCH_NAME} dist
                                    """
                                    stash name: 'x86_64_osx-py37-whl', includes: "dist/*.whl"
                                    arch "dist/*.whl"
                                    // unstash 'VERSION'
                                    // unstash 'GIT_HASH_FILE'
                                    sh """
                                        . ${OSX_CONDA_ACTIVATE_PATH} datatable-py36-with-pandas
                                        make ${MAKE_OPTS} clean
                                        make ${MAKE_OPTS} BRANCH_NAME=${env.BRANCH_NAME} dist
                                    """
                                    stash name: 'x86_64_osx-py36-whl', includes: "dist/*.whl"
                                    arch "dist/*.whl"
                                    // unstash 'VERSION'
                                    // unstash 'GIT_HASH_FILE'
                                    sh """
                                        . ${OSX_CONDA_ACTIVATE_PATH} datatable-py35-with-pandas
                                        make ${MAKE_OPTS} clean
                                        make ${MAKE_OPTS} BRANCH_NAME=${env.BRANCH_NAME} dist
                                    """
                                    stash name: 'x86_64_osx-py35-whl', includes: "dist/*.whl"
                                    arch "dist/*.whl"
                                }
                            }
                        }
                    }
                },
                'Build on ppc64le_centos7': {
                    if (doPPC()) {
                        node(PPC_NODE_LABEL) {
                            final stageDir = 'build-ppc64le_centos7'
                            buildSummary.stageWithSummary('Build on ppc64le_centos7', stageDir) {
                                if(doPPC()) {
                                    cleanWs()
                                    dumpInfo()
                                    dir(stageDir) {
                                        unstash 'datatable-sources'
                                        // unstash 'VERSION'
                                        // unstash 'GIT_HASH_FILE'
                                        sh "make ${MAKE_OPTS} clean && make ${MAKE_OPTS} centos7_build_py37_in_docker"
                                        stash name: 'ppc64le_centos7-py37-whl', includes: "dist/*.whl"
                                        arch "dist/*.whl"
                                        // unstash 'VERSION'
                                        // unstash 'GIT_HASH_FILE'
                                        sh "make ${MAKE_OPTS} clean && make ${MAKE_OPTS} centos7_build_py36_in_docker"
                                        stash name: 'ppc64le_centos7-py36-whl', includes: "dist/*.whl"
                                        arch "dist/*.whl"
                                        // unstash 'VERSION'
                                        // unstash 'GIT_HASH_FILE'
                                        sh "make ${MAKE_OPTS} clean && make ${MAKE_OPTS} centos7_build_py35_in_docker"
                                        stash name: 'ppc64le_centos7-py35-whl', includes: "dist/*.whl"
                                        arch "dist/*.whl"
                                    }
                                }
                            }
                        }
                    } else {
                        echo 'Build on ppc64le_centos7 SKIPPED'
                        markSkipped("Build on ppc64le_centos7")
                    }
                }
            ])
            // Test stages
            if (!params.DISABLE_ALL_TESTS) {
                def testStages = [:]
                testStages <<
                    namedStage('Test x86_64-ubuntu-py37', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-wheels'
                                    test_in_docker("x86_64-ubuntu-py37", "37",
                                                   DOCKER_IMAGE_X86_64_UBUNTU,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-ubuntu-py36', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-wheels'
                                    test_in_docker("x86_64-ubuntu-py36", "36",
                                                   DOCKER_IMAGE_X86_64_UBUNTU,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-ubuntu-py35', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-wheels'
                                    test_in_docker("x86_64-ubuntu-py35", "35",
                                                   DOCKER_IMAGE_X86_64_UBUNTU,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-centos7-py37', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-wheels'
                                    test_in_docker("x86_64-centos7-py37", "37",
                                                   DOCKER_IMAGE_X86_64_CENTOS,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-centos7-py35', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-manylinux-wheels'
                                    test_in_docker("x86_64-centos7-py35", "35",
                                                   DOCKER_IMAGE_X86_64_CENTOS,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-centos7-py37', runPpcTests, { stageName, stageDir ->
                        node(PPC_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le_centos7-py37-whl'
                                    test_in_docker("ppc64le-centos7-py37", "37",
                                                   DOCKER_IMAGE_PPC64LE_CENTOS,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-centos7-py36', runPpcTests, { stageName, stageDir ->
                        node(PPC_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le_centos7-py36-whl'
                                    test_in_docker("ppc64le-centos7-py36", "36",
                                                   DOCKER_IMAGE_PPC64LE_CENTOS,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-centos7-py35', runPpcTests, { stageName, stageDir ->
                        node(PPC_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le_centos7-py35-whl'
                                    test_in_docker("ppc64le-centos7-py35", "35",
                                                   DOCKER_IMAGE_PPC64LE_CENTOS,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py37 with Pandas on x86_64_osx', { stageName, stageDir ->
                        node(OSX_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_osx-py37-whl'
                                    testOSX('datatable-py37-with-pandas', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 with Pandas on x86_64_osx', { stageName, stageDir ->
                        node(OSX_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_osx-py36-whl'
                                    testOSX('datatable-py36-with-pandas', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py35 with Pandas on x86_64_osx', { stageName, stageDir ->
                        node(OSX_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_osx-py35-whl'
                                    testOSX('datatable-py35-with-pandas', needsLargerTest)
                                }
                            }
                        }
                    })
                // Execute defined stages in parallel
                parallel(testStages)
            }
            // Coverage stages
            if (false && !params.DISABLE_COVERAGE) {
                parallel ([
                    'Coverage on x86_64_linux': {
                        node(NODE_LABEL) {
                            final stageDir = 'coverage-x86_64_linux'
                            buildSummary.stageWithSummary('Coverage on x86_64_linux', stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    try {
                                        sh "make ${MAKE_OPTS} CUSTOM_ARGS='${createDockerArgs()}' ubuntu_coverage_py36_with_pandas_in_docker"
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
                        node(OSX_NODE_LABEL) {
                            final stageDir = 'coverage-x86_64_osx'
                            buildSummary.stageWithSummary('Coverage on x86_64_macos', stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    withEnv(OSX_ENV) {
                                        sh """
                                            . ${OSX_CONDA_ACTIVATE_PATH} datatable-py36-with-pandas
                                            make ${MAKE_OPTS} coverage
                                        """
                                    }
                                    testReport "build/coverage-c", "x86_64_osx coverage report for C"
                                    testReport "build/coverage-py", "x86_64_osx coverage report for Python"
                                }
                            }
                        }
                    }
                ])
            }
            // Build sdist
            node(X86_64_BUILD_NODE_LABEL) {
                def stageDir = 'build-sdist'
                buildSummary.stageWithSummary ('Build sdist', stageDir) {
                    cleanWs()
                    dumpInfo()
                    dir (stageDir) {
                        unstash 'datatable-sources'
                        unstash 'VERSION'
                        sh "make ${MAKE_OPTS} ubuntu_build_sdist_in_docker"
                        stash includes: 'dist/*.tar.gz', name: 'sdist-tar'
                        arch "dist/*.tar.gz"
                    }
                }
            }
            // Publish snapshot to S3
            if (doPublish()) {
                node(RELEASE_NODE_LABEL) {
                    def stageDir = 'publish-snapshot'
                    buildSummary.stageWithSummary('Publish Snapshot to S3', stageDir) {
                        cleanWs()
                        dumpInfo()
                        dir(stageDir) {

                            dir('x86_64-centos7') {
                                unstash 'x86_64-manylinux-wheels'
                                s3upDocker {
                                    localArtifact = 'dist/*.whl'
                                    artifactId = 'pydatatable'
                                    version = versionText
                                    keepPrivate = false
                                    platform = 'x86_64-centos7'
                                    isRelease = true
                                }
                            }

                            if (doPPC()) {
                                dir('ppc64le-centos7') {
                                    unstash 'ppc64le_centos7-py37-whl'
                                    unstash 'ppc64le_centos7-py36-whl'
                                    unstash 'ppc64le_centos7-py35-whl'
                                    s3upDocker {
                                        localArtifact = 'dist/*.whl'
                                        artifactId = 'pydatatable'
                                        version = versionText
                                        keepPrivate = false
                                        platform = 'ppc64le-centos7'
                                        isRelease = true
                                    }
                                }
                            }

                            dir('x86_64-osx') {
                                unstash 'x86_64_osx-py37-whl'
                                unstash 'x86_64_osx-py36-whl'
                                unstash 'x86_64_osx-py35-whl'
                                s3upDocker {
                                    localArtifact = 'dist/*.whl'
                                    artifactId = 'pydatatable'
                                    version = versionText
                                    keepPrivate = false
                                    platform = 'x86_64-osx'
                                    isRelease = true
                                }
                            }

                            dir('sdist') {
                                unstash 'sdist-tar'
                                s3upDocker {
                                    localArtifact = 'dist/*.tar.gz'
                                    artifactId = 'pydatatable'
                                    version = versionText
                                    keepPrivate = false
                                    platform = 'any'
                                    isRelease = true
                                }
                            }
                        }
                    }
                }
            }
            // Release to S3 and GitHub
            if (isRelease()) {
                node(RELEASE_NODE_LABEL) {
                    def stageDir = 'release'
                    buildSummary.stageWithSummary('Release', stageDir) {
                        timeout(unit: 'HOURS', time: 24) {
                            input('Promote build?')
                        }
                        cleanWs()
                        dumpInfo()
                        dir(stageDir) {
                            checkout scm
                            unstash 'VERSION'
                            unstash 'x86_64-manylinux-wheels'
                            unstash 'x86_64_osx-py37-whl'
                            unstash 'x86_64_osx-py36-whl'
                            unstash 'x86_64_osx-py35-whl'
                            unstash 'ppc64le_centos7-py37-whl'
                            unstash 'ppc64le_centos7-py36-whl'
                            unstash 'ppc64le_centos7-py35-whl'
                            unstash 'sdist-tar'
                        }
                        docker.withRegistry("https://harbor.h2o.ai", "harbor.h2o.ai") {
                            withCredentials([[$class: 'AmazonWebServicesCredentialsBinding', credentialsId: "awsArtifactsUploader"]]) {
                                docker.image("harbor.h2o.ai/library/s3cmd").inside {
                                    sh """
                                        s3cmd put -P release/dist/*.whl ${S3_URL_STABLE}/datatable-${versionText}/
                                        s3cmd put -P release/dist/*.tar.gz ${S3_URL_STABLE}/datatable-${versionText}/

                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp35-cp35m-linux_ppc64le.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p35-linux_ppc64le.whl
                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp35-cp35m-linux_x86_64.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p35-linux_x86_64.whl
                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp35-cp35m-macosx_10_7_x86_64.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p35-macosx_10_7_x86_64.whl

                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp36-cp36m-linux_ppc64le.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p36-linux_ppc64le.whl
                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp36-cp36m-linux_x86_64.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p36-linux_x86_64.whl
                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp36-cp36m-macosx_10_7_x86_64.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p36-macosx_10_7_x86_64.whl

                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp37-cp37m-linux_ppc64le.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p37-linux_ppc64le.whl
                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp37-cp37m-linux_x86_64.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p37-linux_x86_64.whl
                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp37-cp37m-macosx_10_7_x86_64.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p37-macosx_10_7_x86_64.whl

                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}.tar.gz ${S3_URL_LATEST_STABLE}/datable-0.latest.tar.gz
                                       """
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


def testInDocker(final testTarget, final needsLargerTest) {
    try {
        sh """
            env
            rm -rf src/datatable
            mkdir -p /tmp/cores
            make ${MAKE_OPTS} CUSTOM_ARGS='${createDockerArgs()}' ${testTarget}
        """
    } finally {
        sh 'mkdir -p build/cores && test -n "$(ls -A /tmp/cores)" && mv -f /tmp/cores/*python* build/cores || true'
        // Try to archive all core dumps, but skip error in case of failure
        try {
            arch "build/cores/*python*"
        } catch (ex) { /* ignore */ }
        junit testResults: "build/test-reports/TEST-*.xml", keepLongStdio: true, allowEmptyResults: false
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
//     2-character python version string, such as "36" or "37"
//
// docker_image
//     Name of the docker container where the tests will be run
//
// larg_tests
//     If true, then datatable "large fread tests" will be run as well
//
def test_in_docker(String testtag, String pyver, String docker_image, boolean large_tests) {
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
        if (large_tests) {
            LINK_MAP.each { key, value ->
                docker_args += "-v ${SOURCE_DIR}/${key}:/data/${value} "
            }
            docker_args += "-e DT_LARGE_TESTS_ROOT=/data "
        }
        def python = get_python_for_docker(pyver, docker_image)
        def docker_cmd = ""
        docker_cmd += "cd /dt && ls dist/ && "
        docker_cmd += "virtualenv /tmp/pyenv --python=" + python + " && "
        docker_cmd += "source /tmp/pyenv/bin/activate && "
        docker_cmd += "python -VV && "
        docker_cmd += "pip install --upgrade pip && "
        docker_cmd += "pip install dist/datatable-*-cp" + pyver + "-*.whl && "
        docker_cmd += "pip install -r requirements_tests.txt && "
        docker_cmd += "pip install -r requirements_extra.txt && "
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
        archiveArtifacts artifacts: "build/cores/*", allowEmptyArchive: true
    }
    junit testResults: "build/test-reports/TEST-*.xml", keepLongStdio: true, allowEmptyResults: false
}

def get_python_for_docker(String pyver, String image) {
    if (image == DOCKER_IMAGE_X86_64_UBUNTU) {
        return "python" + pyver[0] + "." + pyver[1]
    }
    if (image == DOCKER_IMAGE_X86_64_CENTOS || image == DOCKER_IMAGE_PPC64LE_CENTOS) {
        if (pyver == "35") return "/opt/h2oai/dai/python/envs/datatable-py35-with-pandas/bin/python3.5"
        if (pyver == "36") return "/opt/h2oai/dai/python/envs/datatable-py36-with-pandas/bin/python3.6"
        if (pyver == "37") return "/opt/h2oai/dai/python/envs/datatable-py37-with-pandas/bin/python3.7"
    }
    if (image == DOCKER_IMAGE_X86_64_MANYLINUX) {
        if (pyver == "35") return "/opt/python/cp35-cp35m/bin/python3.5"
        if (pyver == "36") return "/opt/python/cp36-cp36m/bin/python3.6"
        if (pyver == "37") return "/opt/python/cp37-cp37m/bin/python3.7"
        if (pyver == "38") return "/opt/python/cp38-cp38/bin/python3.8"
    }
    throw new Exception("Unknown python ${pyver} for docker ${image}")
}


def testOSX(final environment, final needsLargerTest) {
    try {
        withEnv(OSX_ENV) {
            sh """
                env
                rm -rf src/datatable
                . ${OSX_CONDA_ACTIVATE_PATH} ${environment}
                pip install --no-cache-dir --upgrade dist/*.whl
                make ${MAKE_OPTS} test_install MODULE=datatable
                make ${MAKE_OPTS} test
            """
        }
    } finally {
        junit testResults: "build/test-reports/TEST-*.xml", keepLongStdio: true, allowEmptyResults: false
    }
}

def isPrJob() {
    return env.CHANGE_BRANCH != null && env.CHANGE_BRANCH != ''
}

def isModified(pattern) {
    def fList
    if (isPrJob()) {
        sh "git fetch --no-tags --progress https://github.com/h2oai/datatable +refs/heads/${env.CHANGE_TARGET}:refs/remotes/origin/${env.CHANGE_TARGET}"
        final String mergeBaseSHA = sh(script: "git merge-base HEAD origin/${env.CHANGE_TARGET}", returnStdout: true).trim()
        fList = sh(script: "git diff --name-only ${mergeBaseSHA}", returnStdout: true).trim()
    } else {
        fList = buildInfo.get().getChangedFiles().join('\n')
    }

    def out = ""
    if (!fList.isEmpty()) {
        out = sh(script: "echo '${fList}' | xargs basename -a | egrep -e '${pattern}' | wc -l", returnStdout: true).trim()
    }
    return !(out.isEmpty() || out == "0")
}

def doPPC() {
    return !isPrJob() || params.FORCE_BUILD_PPC64LE || params.FORCE_ALL_TESTS_IN_PR
}

def doTestPPC64LE() {
    return !params.DISABLE_PPC64LE_TESTS

}

def doPublish() {
    return env.BRANCH_NAME == 'master' || isRelease() || params.FORCE_S3_PUSH
}

def isRelease() {
    return env.BRANCH_NAME.startsWith(RELEASE_BRANCH_PREFIX)
}

def createDockerArgs() {
    def out = ""
    LINK_MAP.each { key, value ->
        out += "-v ${SOURCE_DIR}/${key}:${TARGET_DIR}/${value} "
    }
    out += "-v /tmp/cores:/tmp/cores "
    return out
}

def getHTTPSTargetUrl(final versionText) {
    return "${HTTPS_URL_STABLE}/datatable-${versionText}/"
}

def markSkipped(String stageName) {
    stage (stageName) {
        echo "Stage ${stageName} SKIPPED!"
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
