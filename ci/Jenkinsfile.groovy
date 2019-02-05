#!/usr/bin/groovy
//------------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
X86_64_CENTOS_DOCKER_IMAGE_NAME = "docker.h2o.ai/opsh2oai/datatable-build-x86_64_centos7"
EXPECTED_SHAS = [
    files: [
        'ci/Dockerfile-centos7.in': '4ca4e09a511e0d8af723f2b2222fc818117e0c15',
        'ci/Dockerfile-ubuntu.in': '801cf7132572c9fea31ebcf7b01d86ad76d24873',
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

// Computed version suffix
def CI_VERSION_SUFFIX = utilsLib.getCiVersionSuffix()
// Global project build trigger filled in init stage
def project
// Needs invocation of larger tests
def needsLargerTest
def dockerArgs = createDockerArgs()
// String with current version
def versionText
// String with current git revision
def gitHash

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
                        env.DTBL_GIT_HASH = scmEnv.GIT_COMMIT
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
                            CI_VERSION_SUFFIX = ''
                        }
                        if (env.BRANCH_NAME != 'master' && !env.BRANCH_NAME.startsWith(RELEASE_BRANCH_PREFIX)) {
                            CI_VERSION_SUFFIX = "${env.BRANCH_NAME.replaceAll('(/|_|\\ )', '-')}${CI_VERSION_SUFFIX.split('_').last()}"
                        }
                        env.CI_VERSION_SUFFIX = CI_VERSION_SUFFIX

                        needsLargerTest = isModified("(py_)?fread\\..*|__version__\\.py")
                        if (needsLargerTest) {
                            env.DT_LARGE_TESTS_ROOT = TARGET_DIR
                            manager.addBadge("warning.gif", "Large tests required")
                        }

                        stash includes: "CHANGELOG.md", name: 'CHANGELOG'
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
                    buildSummary.stageWithSummary('Generate version and git files', stageDir) {
                        sh "make ${MAKE_OPTS} centos7_version_in_docker"
                        stash name: 'VERSION', includes: "dist/VERSION.txt"
                        stash name: 'GIT_HASH_FILE', includes: "datatable/__git__.py"
                        arch "dist/VERSION.txt"
                        arch "datatable/__git__.py"
                        versionText = readFile('dist/VERSION.txt').trim()
                        echo "Version is: ${versionText}"
                        sh "make ${MAKE_OPTS} mrproper"
                    }
                    stash 'datatable-sources'
                }
            }
            // Build stages
            parallel([
                'Build on x86_64_centos7': {
                    node(X86_64_BUILD_NODE_LABEL) {
                        final stageDir = 'build-x86_64_centos7'
                        buildSummary.stageWithSummary('Build on x86_64_centos7', stageDir) {
                            cleanWs()
                            dumpInfo()
                            dir(stageDir) {
                                unstash 'datatable-sources'
                                unstash 'VERSION'
                                unstash 'GIT_HASH_FILE'
                                sh "make ${MAKE_OPTS} clean && make ${MAKE_OPTS} centos7_build_py37_in_docker"
                                stash name: 'x86_64_centos7-py37-whl', includes: "dist/*.whl"
                                arch "dist/*.whl"
                                unstash 'VERSION'
                                unstash 'GIT_HASH_FILE'
                                sh "make ${MAKE_OPTS} clean && make ${MAKE_OPTS} centos7_build_py36_in_docker"
                                stash name: 'x86_64_centos7-py36-whl', includes: "dist/*.whl"
                                arch "dist/*.whl"
                                unstash 'VERSION'
                                unstash 'GIT_HASH_FILE'
                                sh "make ${MAKE_OPTS} clean && make ${MAKE_OPTS} centos7_build_py35_in_docker"
                                stash name: 'x86_64_centos7-py35-whl', includes: "dist/*.whl"
                                arch "dist/*.whl"
                            }
                        }
                    }
                },
                'Build on x86_64_macos': {
                    node(OSX_NODE_LABEL) {
                        def stageDir = 'build-x86_64_macos'
                        buildSummary.stageWithSummary('Build on x86_64_macos', stageDir) {
                            cleanWs()
							dumpInfo()
                            dir(stageDir) {
                                unstash 'datatable-sources'
                                withEnv(OSX_ENV) {
                                    unstash 'VERSION'
                                    unstash 'GIT_HASH_FILE'
                                    sh """
                                        . ${OSX_CONDA_ACTIVATE_PATH} datatable-py37-with-pandas
                                        make ${MAKE_OPTS} clean
                                        make ${MAKE_OPTS} BRANCH_NAME=${env.BRANCH_NAME} dist
                                    """
                                    stash name: 'x86_64_osx-py37-whl', includes: "dist/*.whl"
                                    arch "dist/*.whl"
                                    unstash 'VERSION'
                                    unstash 'GIT_HASH_FILE'
                                    sh """
                                        . ${OSX_CONDA_ACTIVATE_PATH} datatable-py36-with-pandas
                                        make ${MAKE_OPTS} clean
                                        make ${MAKE_OPTS} BRANCH_NAME=${env.BRANCH_NAME} dist
                                    """
                                    stash name: 'x86_64_osx-py36-whl', includes: "dist/*.whl"
                                    arch "dist/*.whl"
                                    unstash 'VERSION'
                                    unstash 'GIT_HASH_FILE'
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
                                        unstash 'VERSION'
                                        unstash 'GIT_HASH_FILE'
                                        sh "make ${MAKE_OPTS} clean && make ${MAKE_OPTS} centos7_build_py37_in_docker"
                                        stash name: 'ppc64le_centos7-py37-whl', includes: "dist/*.whl"
                                        arch "dist/*.whl"
                                        unstash 'VERSION'
                                        unstash 'GIT_HASH_FILE'
                                        sh "make ${MAKE_OPTS} clean && make ${MAKE_OPTS} centos7_build_py36_in_docker"
                                        stash name: 'ppc64le_centos7-py36-whl', includes: "dist/*.whl"
                                        arch "dist/*.whl"
                                        unstash 'VERSION'
                                        unstash 'GIT_HASH_FILE'
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
            // Coverage stages
            if (!params.DISABLE_COVERAGE) {
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
            // Test stages
            if (!params.DISABLE_ALL_TESTS) {
                def testStages = [:]
                testStages <<
                    namedStage('Test Py37 with Pandas on x86_64_linux', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py37-whl'
                                    testInDocker('ubuntu_test_py37_with_pandas_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 with Pandas on x86_64_linux', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py36-whl'
                                    testInDocker('ubuntu_test_py36_with_pandas_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py35 with Pandas on x86_64_linux', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py35-whl'
                                    testInDocker('ubuntu_test_py35_with_pandas_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 with Numpy on x86_64_linux', !isPrJob() || params.FORCE_ALL_TESTS_IN_PR, { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py36-whl'
                                    testInDocker('ubuntu_test_py36_with_numpy_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 on x86_64_linux', !isPrJob() || params.FORCE_ALL_TESTS_IN_PR, { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py36-whl'
                                    testInDocker('ubuntu_test_py36_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py37 with Pandas on x86_64_centos7', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary('Test Py37 with Pandas on x86_64_centos7', stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py37-whl'
                                    testInDocker('centos7_test_py37_with_pandas_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 with Pandas on x86_64_centos7', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary('Test Py36 with Pandas on x86_64_centos7', stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py36-whl'
                                    testInDocker('centos7_test_py36_with_pandas_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py35 with Pandas on x86_64_centos7', { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary('Test Py35 with Pandas on x86_64_centos7', stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py35-whl'
                                    testInDocker('centos7_test_py35_with_pandas_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 with Numpy on x86_64_centos7', !isPrJob() || params.FORCE_ALL_TESTS_IN_PR, { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py36-whl'
                                    testInDocker('centos7_test_py36_with_numpy_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 on x86_64_centos7', !isPrJob() || params.FORCE_ALL_TESTS_IN_PR, { stageName, stageDir ->
                        node(NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_centos7-py36-whl'
                                    testInDocker('centos7_test_py36_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py37 with Pandas on ppc64le_centos7', doPPC() && doTestPPC64LE(), { stageName, stageDir ->
                        node(PPC_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le_centos7-py37-whl'
                                    testInDocker('centos7_test_py37_with_pandas_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 with Pandas on ppc64le_centos7', doPPC() && doTestPPC64LE(), { stageName, stageDir ->
                        node(PPC_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le_centos7-py36-whl'
                                    testInDocker('centos7_test_py36_with_pandas_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py35 with Pandas on ppc64le_centos7', doPPC() && doTestPPC64LE(), { stageName, stageDir ->
                        node(PPC_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le_centos7-py35-whl'
                                    testInDocker('centos7_test_py35_with_pandas_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 with Numpy on ppc64le_centos7', !isPrJob() && doPPC() && doTestPPC64LE() || params.FORCE_ALL_TESTS_IN_PR, { stageName, stageDir ->
                        node(PPC_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le_centos7-py36-whl'
                                    testInDocker('centos7_test_py36_with_numpy_in_docker', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 on ppc64le_centos7', !isPrJob() && doPPC() && doTestPPC64LE() || params.FORCE_ALL_TESTS_IN_PR, { stageName, stageDir ->
                        node(PPC_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le_centos7-py36-whl'
                                    testInDocker('centos7_test_py36_in_docker', needsLargerTest)
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
                    }) <<
                    namedStage('Test Py36 with Numpy on x86_64_osx', !isPrJob() || params.FORCE_ALL_TESTS_IN_PR, { stageName, stageDir ->
                        node(OSX_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_osx-py36-whl'
                                    testOSX('datatable-py36-with-numpy', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test Py36 on x86_64_osx', !isPrJob() || params.FORCE_ALL_TESTS_IN_PR, { stageName, stageDir ->
                        node(OSX_NODE_LABEL) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64_osx-py36-whl'
                                    testOSX('datatable-py36', needsLargerTest)
                                }
                            }
                        }
                    })
                // Execute defined stages in parallel
                parallel(testStages)
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
                                unstash 'x86_64_centos7-py37-whl'
                                unstash 'x86_64_centos7-py36-whl'
                                unstash 'x86_64_centos7-py35-whl'
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
                            unstash 'CHANGELOG'
                            unstash 'VERSION'
                            unstash 'x86_64_centos7-py37-whl'
                            unstash 'x86_64_centos7-py36-whl'
                            unstash 'x86_64_centos7-py35-whl'
                            unstash 'x86_64_osx-py36-whl'
                            unstash 'x86_64_osx-py35-whl'
                            unstash 'ppc64le_centos7-py37-whl'
                            unstash 'ppc64le_centos7-py36-whl'
                            unstash 'ppc64le_centos7-py35-whl'
                            unstash 'sdist-tar'
                        }
                        docker.withRegistry("https://docker.h2o.ai", "docker.h2o.ai") {
                            withCredentials([[$class: 'AmazonWebServicesCredentialsBinding', credentialsId: "awsArtifactsUploader"]]) {
                                docker.image("docker.h2o.ai/s3cmd").inside {
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
                                        #s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}-cp37-cp37m-macosx_10_7_x86_64.whl ${S3_URL_LATEST_STABLE}/datable-0.latest-p37-macosx_10_7_x86_64.whl
                                        
                                        s3cmd cp -f ${S3_URL_STABLE}/datatable-${versionText}/datatable-${versionText}.tar.gz ${S3_URL_LATEST_STABLE}/datable-0.latest.tar.gz
                                       """
                                }
                            }
                            docker.image('docker.h2o.ai/opsh2oai/hub').inside("--init") {
                                withCredentials([file(credentialsId: RSA_CRED_ID, variable: 'ID_RSA_PATH'), file(credentialsId: GITCONFIG_CRED_ID, variable: 'GITCONFIG_PATH'), string(credentialsId: CREDS_ID, variable: 'GITHUB_TOKEN')]) {
                                    final def releaseMsgFile = "release-msg.md"
                                    def releaseMsg = """v${versionText}

${project.getChangelogPartForVersion(versionText, "release/CHANGELOG.md")}
---
${project.getReleaseDownloadLinksText("release/dist", "${getHTTPSTargetUrl(versionText)}")}
"""
                                    writeFile(file: "release/${releaseMsgFile}", text: releaseMsg)
                                    sh """
                                        mkdir -p ~/.ssh
                                        cp \${ID_RSA_PATH} ~/.ssh/id_rsa
                                        cp \${GITCONFIG_PATH} ~/.gitconfig
                                        cd release
                                        hub release create -f ${releaseMsgFile} \$(find dist/ \\( -name '*.whl' -o -name '*.tar.gz' \\) -exec echo -a {} \\;) "v${versionText}"
                                    """
                                    echo readFile("release/${releaseMsgFile}").trim()
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
            rm -rf datatable
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

def testOSX(final environment, final needsLargerTest) {
    try {
        withEnv(OSX_ENV) {
            sh """
                env
                rm -rf datatable
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
