#!/bin/bash
# Stop on error.
#set -e
#set -x

function s3ToHttps() {
    local t=$1
    echo "$t" | sed -e 's|s3://\([^/]*\)/\(.*\)|https://\1.s3.amazonaws.com/\2|'
}

if [ $# -lt 2 ]; then
cat <<EOF
The latest stable build link generator.

 $0 <S3 TARGET> <FORWARD FILE> [<DRY RUN COMMAND>]

 Parameters:
   S3 TARGET       - S3 location of forward target (e.g., s3://h2o-release/datatable/stable/datatable-1.2.3.tgz
   FORWARD FILE    - S3 location of forward file (e.g., s3://h2o-release/datatable/stable/latest-datatable.html
   DRY RUN COMMAND - optional command to prepend before executing s3cmd

EOF
exit
fi
tmpdir=./.dtbl.tmp
mkdir -p $tmpdir

S3_TARGET="$1"
HTTPS_TARGET="$(s3ToHttps "${S3_TARGET}")"
S3_LINK="$2"
HTTPS_LINK="$(s3ToHttps "${S3_LINK}")"
LINK_FILE="$(basename "${S3_LINK}")"
Q="$3"

# Output all other links
cat <<EOF > "${tmpdir}/${LINK_FILE}"
<head>
<meta http-equiv="refresh" content="0; url=${HTTPS_TARGET}" />
</head>
EOF

# Upload
S3CMD_OPTS="--dry-run"
S3CMD_OPTS=
S3CMD="s3cmd $S3CMD_OPTS"
# Upload first stable link
$Q ${S3CMD} --acl-public put "${tmpdir}/${LINK_FILE}" "${S3_LINK}"
echo -en "\033[0;91m"
cat <<EOF
Forward generated:
  From: ${HTTPS_LINK}
  To  : ${HTTPS_TARGET}
EOF
echo -en "\033[0;39m"
