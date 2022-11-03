#!/usr/bin/python

import os
import sys

if len(sys.argv) == 1:
    VERSION = open("VERSION", "rt").read()
    VERSION = VERSION.split(".")
    last_release_branch = "%d.%d" % (int(VERSION[0]), int(VERSION[1]) - 1)
else:
    last_release_branch = sys.argv[1]

print("Generating /tmp/log.txt with changes not in %s branch" % last_release_branch)

os.system(
    f'git log --reverse -v v{last_release_branch}.0..HEAD . ":(exclude)autotest" ":(exclude)doc" ":(exclude).github" > /tmp/log_raw.txt'
)
os.system(
    f'git log --no-merges --reverse -v v{last_release_branch}.0..release/{last_release_branch} . ":(exclude)autotest" ":(exclude)doc" ":(exclude).github" > /tmp/log_bugfixes.txt'
)


class Commit(object):
    def __init__(self):
        self.metadata = ""
        self.message = ""


def get_commits(filename):
    commits = []
    commit = None
    for l in open(filename, "rt").readlines():
        if l.startswith("commit "):
            commit = Commit()
            commits.append(commit)
            commit.metadata += l
        elif commit.message == "" and l == "\n":
            commit.message += l
        elif commit.message == "":
            commit.metadata += l
        else:
            commit.message += l
    return commits


raw_commits = get_commits("/tmp/log_raw.txt")
bugfixes_commits = get_commits("/tmp/log_bugfixes.txt")
set_bugfixes_commit_messages = set()
for commit in bugfixes_commits:
    set_bugfixes_commit_messages.add(commit.message)

with open("/tmp/log.txt", "wt") as f:
    for commit in raw_commits:
        is_bugfix = False
        message = commit.message.split("\n")[1]
        for bugfix_commit_message in set_bugfixes_commit_messages:
            if message in bugfix_commit_message:
                is_bugfix = True
                break
        if not is_bugfix:
            f.write(commit.metadata)
            f.write(commit.message)
