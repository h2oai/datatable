#!/usr/bin/groovy
//------------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
//------------------------------------------------------------------------------

GENERIC_VERSION_REGEX = /^### \[v(\d+\.)+\d\].*/




def getReleaseDownloadLinksText(final folder, final s3PathPrefix) {
    def files = sh(script: "cd ${folder} && find . \\( -name '*.whl' -o -name '*.tar.gz' \\) -printf '%P\n'", returnStdout: true).trim().readLines()
    def resultLines = ['## Download links ##', '']
    files.each { file ->
        resultLines += "- [${file}](${s3PathPrefix}/${file})"
    }
    return resultLines.join('\n').trim()

}

return this

