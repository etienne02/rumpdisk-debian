#! /bin/sh

# This script prunes ~1.1G of currently unused files.

set -ex

cd buildrump.sh/src

rm -rf crypto # 169M
rm -rf distrib # 30M

mkdir -p external-safe/bsd external-safe/historical
cp -a external/bsd/byacc external/bsd/flex external/bsd/mdocml external-safe/bsd
cp -a external/historical/nawk external-safe/historical
rm -rf external # 787M
mv external-safe external

rm -rf games # 10M

mkdir -p share-safe
cp -a share/mk share-safe
rm -rf share # 36M
mv share-safe share

mkdir -p sys/external-safe
cp -a sys/external/bsd sys/external/isc sys/external-safe
rm -rf sys/external # 29M
mv sys/external-safe sys/external

