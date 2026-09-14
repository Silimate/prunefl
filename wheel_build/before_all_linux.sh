set -e
set -x
if command -v yum; then
    yum install -y devtoolset-11
fi
