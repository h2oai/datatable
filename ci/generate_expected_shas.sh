#! /bin/bash

####################################################################################
# Helper script to generate snippet with EXPECTED_SHAS for the Jenkinsfile,groovy. #
####################################################################################

set -e

read shacentos filecentos shaubuntu fileubuntu <<< $(shasum Dockerfile-* | tr '\n' ' ')
echo """EXPECTED_SHAS = [
    files: [
        'ci/${filecentos}': '${shacentos}',
        'ci/${fileubuntu}': '${shaubuntu}',
    ]
]"""
