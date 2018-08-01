#!/usr/bin/groovy
//------------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
//------------------------------------------------------------------------------

@Library('test-shared-library@mr/ita/190-build-summary-emailer') _

import ai.h2o.ci.buildsummary.StagesSummary
import ai.h2o.ci.buildsummary.DetailsSummary
import ai.h2o.ci.BuildResult

properties([
        buildDiscarder(logRotator(artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '', numToKeepStr: '25')),
        parameters([
                string(name: 'registry', defaultValue: 'docker.h2o.ai', description: 'Docker registry to push images to'),
                booleanParam(name: 'publish', defaultValue: true, description: 'If true, publish the docker image'),
        ]),
        pipelineTriggers([])
])

BuildResult result = BuildResult.FAILURE

// initialize build summary
buildSummary('https://github.com/h2oai/datatable', true)
// setup custom DetailsSummary
DetailsSummary detailsSummary = new DetailsSummary()
detailsSummary.setEntry(this, 'Publish', params.publish ? 'Yes' : 'No')
buildSummary.get().addDetailsSummary(this, detailsSummary)
// use default StagesSummary implementation
buildSummary.get().addStagesSummary(this, new StagesSummary())

try {
    node('docker && mr-0x5') {
        buildSummary.stageWithSummary("Build images for x86_64") {
            dir('centos7') {
                checkout scm
                sh """
                    make mrproper
                    make centos7_docker_build CONTAINER_NAME_SUFFIX=
                """
                arch "*.tag"
            }
            dir ('ubuntu') {
                checkout scm
                sh """
                    make mrproper
                    make ubuntu_docker_build CONTAINER_NAME_SUFFIX= OS_NAME=ubuntu
                """
                arch "*.tag"
            }
        }

        if (params.publish) {
            buildSummary.stageWithSummary("Publish images for x86_64") {
                withCredentials([usernamePassword(credentialsId: "${params.registry}", usernameVariable: 'REGISTRY_USERNAME', passwordVariable: 'REGISTRY_PASSWORD')]) {
                    sh """
                        docker login -u $REGISTRY_USERNAME -p $REGISTRY_PASSWORD ${params.registry}
                    """
                    dir('centos7') {
                        sh """
                            make centos7_docker_publish CONTAINER_NAME_SUFFIX=
                        """
                    }
                    dir('ubuntu') {
                        sh """
                             make ubuntu_docker_publish CONTAINER_NAME_SUFFIX= OS_NAME=ubuntu
                        """
                    }
                    echo "###### Docker image docker.h2o.ai/opsh2oai/datatable-build-x86_64_centos7 built and pushed. ######"
                }
            }
        }
    }

    node('ibm-power') {
        buildSummary.stageWithSummary("Build image for ppc64le") {
            dir('centos7') {
                checkout scm
                sh """
                    make mrproper
                    make centos7_docker_build CONTAINER_NAME_SUFFIX=
                    make centos7_docker_publish CONTAINER_NAME_SUFFIX=
                """
                arch "*.tag"
            }
        }

        // we cannot publish to internal registry from this node
    }
    result = BuildResult.SUCCESS
} finally {
    sendEmailNotif(result, buildSummary.get().toEmail(this), ['michalr@h2o.ai'])
}
