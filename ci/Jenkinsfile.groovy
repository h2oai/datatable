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
NODE_LINUX_BUILD = "buildMachine"
NODE_LINUX_TESTS = 'docker && !mr-0xc8'
NODE_MACOS = 'osx'
NODE_PPC = 'ibm-power'
NODE_RELEASE = 'master'


EXPECTED_SHAS = [
    files: [
        'ci/Dockerfile-centos7.in': '0dbfd08d5857fdaa5043ffae386895d4fe524a47',
        'ci/Dockerfile-ubuntu.in': '8dbbd6afe03062befa391c20be95daf58caee4ac',
    ]
]

// Paths should be absolute
S3_URL_STABLE = "s3://h2o-release/datatable/stable"
HTTPS_URL_STABLE = "https://h2o-release.s3.amazonaws.com/datatable/stable"
// Data map for linking into container
LINK_MAP = [
    "Data"     : "h2oai-benchmarks/Data",
    "smalldata": "h2o-3/smalldata",
    "bigdata"  : "h2o-3/bigdata",
    "fread"    : "h2o-3/fread",
]


DOCKER_IMAGE_PPC64LE_MANYLINUX = "quay.io/pypa/manylinux2014_ppc64le"
DOCKER_IMAGE_X86_64_MANYLINUX = "quay.io/pypa/manylinux2010_x86_64"
DOCKER_IMAGE_X86_64_CENTOS = "harbor.h2o.ai/opsh2oai/datatable-build-x86_64_centos7:0.8.0-master.9"
DOCKER_IMAGE_X86_64_UBUNTU = "harbor.h2o.ai/opsh2oai/datatable-build-x86_64_ubuntu:0.8.0-master.9"

DOCKER_IMAGE_PPC64LE_CENTOS = "harbor.h2o.ai/opsh2oai/datatable-build-ppc64le_centos7:0.8.0-master.9"

// Note: global variables must be declared without `def`
//       see https://stackoverflow.com/questions/6305910

// Needs invocation of larger tests
needsLargerTest = false
// String with current version (TODO: remove?)
versionText = "unknown"

isPrJob = !(env.CHANGE_BRANCH == null || env.CHANGE_BRANCH == '')
doExtraTests = (!isPrJob || params.FORCE_ALL_TESTS_IN_PR) && !params.DISABLE_ALL_TESTS
doPpcTests = true || doExtraTests && !params.DISABLE_PPC64LE_TESTS
doPpcBuild = doPpcTests || params.FORCE_BUILD_PPC64LE
doCoverage = !params.DISABLE_COVERAGE && false   // disable for now

DT_RELEASE = ""
DT_BUILD_SUFFIX = ""
DT_BUILD_NUMBER = ""

//////////////
// PIPELINE //
//////////////
properties([
    parameters([
        booleanParam(name: 'FORCE_BUILD_PPC64LE',   defaultValue: false, description: '[BUILD] Trigger build of PPC64le artifacts.'),
        booleanParam(name: 'DISABLE_ALL_TESTS',     defaultValue: false, description: '[BUILD] Disable all tests.'),
        booleanParam(name: 'DISABLE_PPC64LE_TESTS', defaultValue: false, description: '[BUILD] Disable PPC64LE tests.'),
        booleanParam(name: 'DISABLE_COVERAGE',      defaultValue: false, description: '[BUILD] Disable coverage.'),
        booleanParam(name: 'FORCE_ALL_TESTS_IN_PR', defaultValue: false, description: '[BUILD] Trigger all tests even for PR.'),
        booleanParam(name: 'FORCE_S3_PUSH',         defaultValue: false, description: '[BUILD] Publish to S3 regardless of current branch.')
    ]),
    buildDiscarder(logRotator(artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '180', numToKeepStr: ''))
])

ansiColor('xterm') {
    timestamps {
        if (isPrJob) {
            cancelPreviousBuilds()
        }
        timeout(time: 180, unit: 'MINUTES') {
            // Checkout stage
            node(NODE_LINUX_BUILD) {
                def stageDir = 'checkout'
                dir (stageDir) {
                    buildSummary.stageWithSummary('Checkout and Setup Env', stageDir) {
                        deleteDir()
                        def scmEnv = checkout scm
                        sh """
                            set +x
                            echo 'env.BRANCH_NAME   = ${env.BRANCH_NAME}'
                            echo 'env.CHANGE_BRANCH = ${env.CHANGE_BRANCH}'
                            echo 'env.CHANGE_ID     = ${env.CHANGE_ID}'
                            echo 'env.CHANGE_TARGET = ${env.CHANGE_TARGET}'
                            echo 'env.CHANGE_SOURCE = ${env.CHANGE_SOURCE}'
                            echo 'env.CHANGE_FORK   = ${env.CHANGE_FORK}'
                            echo 'scm.GIT_BRANCH    = ${scmEnv.GIT_BRANCH}'
                            echo 'scm.CHANGE_BRANCH = ${scmEnv.CHANGE_BRANCH}'
                            echo 'scm.CHANGE_SOURCE = ${scmEnv.CHANGE_SOURCE}'
                            echo 'isPrJob      = ${isPrJob}'
                            echo 'doExtraTests = ${doExtraTests}'
                            echo 'doPpcBuild   = ${doPpcBuild}'
                            echo 'doPpcTests   = ${doPpcTests}'
                            echo 'doCoverage   = ${doCoverage}'
                        """

                        env.BRANCH_NAME = scmEnv.GIT_BRANCH.replaceAll('origin/', '').replaceAll('/', '-')

                        if (doPpcBuild) {
                            manager.addBadge("success.gif", "PPC64LE build triggered.")
                        }
                        if (doPublish()) {
                            manager.addBadge("package.gif", "Publish to S3.")
                        }

                        buildInfo(env.BRANCH_NAME, isRelease())

                        if (isRelease()) {
                            DT_RELEASE = 'True'
                        }
                        else if (env.BRANCH_NAME == 'master') {
                            DT_BUILD_NUMBER = env.BUILD_ID
                        }
                        else {
                            DT_BUILD_SUFFIX = env.BRANCH_NAME.replaceAll('[^\\w]+', '') + "." + env.BUILD_ID
                        }

                        sh """
                            set +x
                            echo 'DT_RELEASE      = ${DT_RELEASE}'
                            echo 'DT_BUILD_NUMBER = ${DT_BUILD_NUMBER}'
                            echo 'DT_BUILD_SUFFIX = ${DT_BUILD_SUFFIX}'
                        """
                        needsLargerTest = isModified("c/(read|csv)/.*")
                        if (needsLargerTest) {
                            env.DT_LARGE_TESTS_ROOT = "/tmp/pydatatable_large_data"
                            manager.addBadge("warning.gif", "Large tests required")
                        }
                        sh """
                            set +x
                            echo 'needsLargerTests = ${needsLargerTest}'
                        """

                        docker.image(DOCKER_IMAGE_X86_64_CENTOS).inside {
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
                                -c "env && /opt/python/cp36-cp36m/bin/python3.6 ci/ext.py sdist"
                        """
                        sh "cat src/datatable/_build_info.py"
                        arch "src/datatable/_build_info.py"
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
                    node(NODE_LINUX_BUILD) {
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
                                            /opt/python/cp35-cp35m/bin/python3.5 ci/ext.py wheel --audit && \
                                            /opt/python/cp36-cp36m/bin/python3.6 ci/ext.py wheel --audit && \
                                            /opt/python/cp37-cp37m/bin/python3.7 ci/ext.py wheel --audit && \
                                            /opt/python/cp38-cp38/bin/python3.8 ci/ext.py wheel --audit && \
                                            ls -la dist"
                                """
                                stash name: 'x86_64-manylinux-wheels', includes: "dist/*.whl"
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
                                        . /Users/jenkins/anaconda/bin/activate datatable-py37-with-pandas
                                        python ci/ext.py wheel
                                        . /Users/jenkins/anaconda/bin/activate datatable-py36-with-pandas
                                        python ci/ext.py wheel
                                        . /Users/jenkins/anaconda/bin/activate datatable-py35-with-pandas
                                        python ci/ext.py wheel
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

                                // "sed" line below applies patch https://github.com/pypa/auditwheel/pull/213
                                // It can be removed when the PR is merged and new ppc64le image is available
                                // Since it modifies the file owned by root, we cannot use the standard
                                // approach of starting docker under the jenkins user. Instead, we start as
                                // a root, modify the file, then create the jenkins user with the correct
                                // uid/gid, and finally switch to that user's context.
                                //
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    sh """
                                        docker run --rm --init \
                                            -v `pwd`:/dot \
                                            -e DT_RELEASE=${DT_RELEASE} \
                                            -e DT_BUILD_SUFFIX=${DT_BUILD_SUFFIX} \
                                            -e DT_BUILD_NUMBER=${DT_BUILD_NUMBER} \
                                            --entrypoint /bin/bash \
                                            ${DOCKER_IMAGE_PPC64LE_MANYLINUX} \
                                            -c "cd /dot && \
                                                ls -la && \
                                                ls -la src/datatable && \
                                                sed -i \\\"s/if 'ld-linux' in lib:/if 'ld-linux' in lib or 'ld64.so' in lib:/\\\" /opt/_internal/cpython-3.7.6/lib/python3.7/site-packages/auditwheel/policy/external_references.py && \
                                                groupadd -g `id -g` jenkins && \
                                                useradd -u `id -u` -g jenkins jenkins && \
                                                su jenkins && \
                                                /opt/python/cp35-cp35m/bin/python3.5 ci/ext.py wheel --audit && \
                                                /opt/python/cp36-cp36m/bin/python3.6 ci/ext.py wheel --audit && \
                                                /opt/python/cp37-cp37m/bin/python3.7 ci/ext.py wheel --audit && \
                                                /opt/python/cp38-cp38/bin/python3.8 ci/ext.py wheel --audit && \
                                                ls -la dist"
                                    """
                                    stash name: 'ppc64le-manylinux-wheels', includes: "dist/*.whl"
                                    arch "dist/*.whl"
                                }
                            }
                        }
                    } else {
                        echo 'Build on ppc64le-manylinux SKIPPED'
                        markSkipped("Build on ppc64le-manylinux")
                    }
                }
            ])
            // Test stages
            if (!params.DISABLE_ALL_TESTS) {
                def testStages = [:]
                testStages <<
                    namedStage('Test x86_64-ubuntu-py37', { stageName, stageDir ->
                        node(NODE_LINUX_TESTS) {
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
                        node(NODE_LINUX_TESTS) {
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
                        node(NODE_LINUX_TESTS) {
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
                        node(NODE_LINUX_TESTS) {
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
                        node(NODE_LINUX_TESTS) {
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
                    namedStage('Test ppc64le-centos7-py37', doPpcTests, { stageName, stageDir ->
                        node(NODE_PPC) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le-manylinux-wheels'
                                    test_in_docker("ppc64le-centos7-py37", "37",
                                                   DOCKER_IMAGE_PPC64LE_MANYLINUX,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-centos7-py36', doPpcTests, { stageName, stageDir ->
                        node(NODE_PPC) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le-manylinux-wheels'
                                    test_in_docker("ppc64le-centos7-py36", "36",
                                                   DOCKER_IMAGE_PPC64LE_MANYLINUX,
                                                   needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test ppc64le-centos7-py35', doPpcTests, { stageName, stageDir ->
                        node(NODE_PPC) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'ppc64le-manylinux-wheels'
                                    test_in_docker("ppc64le-centos7-py35", "35",
                                                   DOCKER_IMAGE_PPC64LE_MANYLINUX,
                                                   needsLargerTest)
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
                                    test_macos('37', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-macos-py36', { stageName, stageDir ->
                        node(NODE_MACOS) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-macos-wheels'
                                    test_macos('36', needsLargerTest)
                                }
                            }
                        }
                    }) <<
                    namedStage('Test x86_64-macos-py35', { stageName, stageDir ->
                        node(NODE_MACOS) {
                            buildSummary.stageWithSummary(stageName, stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    unstash 'x86_64-macos-wheels'
                                    test_macos('35', needsLargerTest)
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
                        node(NODE_LINUX_TESTS) {
                            final stageDir = 'coverage-x86_64_linux'
                            buildSummary.stageWithSummary('Coverage on x86_64_linux', stageDir) {
                                cleanWs()
                                dumpInfo()
                                dir(stageDir) {
                                    unstash 'datatable-sources'
                                    try {
                                        sh "make CUSTOM_ARGS='${createDockerArgs()}' ubuntu_coverage_py36_with_pandas_in_docker"
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
                                        . /Users/jenkins/anaconda/bin/activate datatable-py36-with-pandas
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
            if (doPublish()) {
                node(NODE_RELEASE) {
                    def stageDir = 'publish-snapshot'
                    buildSummary.stageWithSummary('Publish Snapshot to S3', stageDir) {
                        cleanWs()
                        dumpInfo()
                        dir(stageDir) {
                            sh "rm -rf dist"
                            unstash 'x86_64-manylinux-wheels'
                            unstash 'x86_64-macos-wheels'
                            unstash 'sdist'
                            if (doPpcBuild) {
                                unstash 'ppc64le-manylinux-wheels'
                            }
                            // FIXME: ${versionText} is an undefined variable.
                            //        It has to be extracted from the filenames
                            //        of the wheels/sdist:
                            //
                            //            dist/datatable-${version}.tar.gz
                            //            dist/datatable-${version}-*.whl
                            //
                            //        At this point we'd also want to verify
                            //        that the versions on all files are the
                            //        same, and that if we are in release mode
                            //        the version contains only digits and dots.
                            //
                            s3upDocker {
                                localArtifact = 'dist/*'
                                artifactId = 'datatable'
                                version = versionText
                                keepPrivate = false
                                isRelease = isRelease()
                            }
                       }
                    }
                }
            }
            // Release to S3 and GitHub
            if (isRelease()) {
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
                            unstash 'x86_64-macos-wheels'
                            unstash 'ppc64le-manylinux-wheels'
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
                docker_args += "-v /home/0xdiag/${key}:/data/${value} "
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
        docker_cmd += "(pip install -r requirements_extra.txt || true) && "
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
    if (image == DOCKER_IMAGE_X86_64_UBUNTU) {
        return "python" + pyver[0] + "." + pyver[1]
    }
    if (image == DOCKER_IMAGE_X86_64_CENTOS || image == DOCKER_IMAGE_PPC64LE_CENTOS) {
        if (pyver == "35") return "/opt/h2oai/dai/python/envs/datatable-py35-with-pandas/bin/python3.5"
        if (pyver == "36") return "/opt/h2oai/dai/python/envs/datatable-py36-with-pandas/bin/python3.6"
        if (pyver == "37") return "/opt/h2oai/dai/python/envs/datatable-py37-with-pandas/bin/python3.7"
    }
    if (image == DOCKER_IMAGE_X86_64_MANYLINUX || image == DOCKER_IMAGE_PPC64LE_MANYLINUX) {
        if (pyver == "35") return "/opt/python/cp35-cp35m/bin/python3.5"
        if (pyver == "36") return "/opt/python/cp36-cp36m/bin/python3.6"
        if (pyver == "37") return "/opt/python/cp37-cp37m/bin/python3.7"
        if (pyver == "38") return "/opt/python/cp38-cp38/bin/python3.8"
    }
    throw new Exception("Unknown python ${pyver} for docker ${image}")
}


def test_macos(String pyver, boolean needsLargerTest) {
    try {
        sh """
            rm -f /tmp/cores/*
            env
            . /Users/jenkins/anaconda/bin/activate datatable-py${pyver}-with-pandas
            pip install --upgrade pip
            pip install dist/datatable-*-cp${pyver}-*.whl
            pip install -r requirements_tests.txt
            pip install -r requirements_extra.txt
            pip freeze
            python -c 'import datatable; print(datatable.__file__)'
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


def isModified(pattern) {
    def fList
    if (isPrJob) {
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


def doPublish() {
    return env.BRANCH_NAME == 'master' || isRelease() || params.FORCE_S3_PUSH
}

def isRelease() {
    return env.BRANCH_NAME.startsWith("rel-")
}

def createDockerArgs() {
    def out = ""
    LINK_MAP.each { key, value ->
        out += "-v /home/0xdiag/${key}:/tmp/pydatatable_large_data/${value} "
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
