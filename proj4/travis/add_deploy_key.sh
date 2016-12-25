#!/bin/bash

openssl aes-256-cbc -K $encrypted_38e0a668034a_key -iv $encrypted_38e0a668034a_iv -in travis/projdocs-private.key.enc -out travis/projdocs-private.key -d
cp travis/projdocs-private.key ~/.ssh/id_rsa
chmod 600 ~/.ssh/id_rsa
echo -e "Host *\n\tStrictHostKeyChecking no\n" > ~/.ssh/config


