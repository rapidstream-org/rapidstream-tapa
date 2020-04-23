#!/bin/bash
set -e
# check required parameters
: ${GPG_KEY:?"required but not set"}
: ${SSH_KEY:?"required but not set"}
: ${BUILD_DIR:?"required but not set"}
: ${CACHE_DIR:?"required but not set"}
: ${LABEL:?"required but not set"}
: ${GITHUB_SHA:?"required but not set"}

# install packaing dependencies
sudo apt-get install -y apt-utils gzip gnupg moreutils rsync

# setup parameters
arch=amd64
codename="$(grep --perl --only '(?<=UBUNTU_CODENAME=).+' /etc/os-release)"
repo_dir="$(pwd)"
src_dir="${repo_dir}/${BUILD_DIR}"
dest_dir="$(mktemp --directory)"
cache_dir="${repo_dir}/${CACHE_DIR}"

# setup ssh
mkdir -p -m 700 "${HOME}/.ssh"
(umask 077 && echo "${SSH_KEY}" >"${HOME}/.ssh/id_ed25519")

# setup gpg
export GNUPGHOME="$(mktemp --directory)"
mkdir -p "${cache_dir}"
echo "${GPG_KEY}" | gpg --no-tty --batch --allow-secret-key-import --import -

# clone gh-pages
git clone --single-branch --branch gh-pages \
  "git@github.com:${GITHUB_REPOSITORY}" "${dest_dir}"

# start packaging
pushd "${dest_dir}"

mkdir -p "pool/main/${codename}"
rsync --recursive --include "*.deb" --exclude "*" \
  --link-dest "${src_dir}/" "${src_dir}/" "pool/main/${codename}"
mkdir -p "dists/${codename}/main/binary-${arch}"

apt-ftparchive packages --db "${cache_dir}/${codename}.db" \
  "pool/main/${codename}" |
  sponge "dists/${codename}/main/binary-${arch}/Packages"
gzip --stdout <"dists/${codename}/main/binary-${arch}/Packages" |
  sponge "dists/${codename}/main/binary-${arch}/Packages.gz"
apt-ftparchive contents --db "${cache_dir}/${codename}.db" \
  "pool/main/${codename}" |
  sponge "dists/${codename}/main/Contents-${arch}"
gzip --stdout "dists/${codename}/main/Contents-${arch}" |
  sponge "dists/${codename}/main/Contents-${arch}.gz"
apt-ftparchive release "dists/${codename}/main/binary-${arch}" |
  sponge "dists/${codename}/main/binary-${arch}/Release"
apt-ftparchive release \
  -o="APT::FTPArchive::Release::Codename=${codename}" \
  -o='APT::FTPArchive::Release::Components=main' \
  -o="APT::FTPArchive::Release::Label=${LABEL}" \
  -o="APT::FTPArchive::Release::Architectures=${arch}" \
  "dists/${codename}" >"${dest_dir}/dists/${codename}/Release"

gpg --no-tty --batch --armor --yes --output "dists/${codename}/Release.gpg" \
  --detach-sign "dists/${codename}/Release"
gpg --no-tty --batch --armor --yes --output "dists/${codename}/InRelease" \
  --detach-sign --clearsign "dists/${codename}/Release"

# upload gh-pages
git add --all
git -c user.name='Blaok' -c user.email='i@blaok.me' \
  commit -m "actions update for ${codename}@${GITHUB_SHA}"
git push

# cleanup gpg and ssh
rm -rf "${GNUPGHOME}" "${HOME}/.ssh/id_ed25519"
