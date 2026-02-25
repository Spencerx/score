#!/bin/bash

if [[ -d ../.git ]]; then
  cd ..
elif [[ -d score/.git ]]; then
  cd score
fi

if [[ ! -d src/addons ]]; then
  echo "Must call this script from score root folder"
  exit 1
fi

git pull --rebase --autostash
git submodule update --init --recursive

(
  cd src/addons

  for addon in ./*/; do         
  (
    cd $addon
    pwd
    git pull --rebase --autostash
    git submodule update --init --recursive
  )
  done
)

