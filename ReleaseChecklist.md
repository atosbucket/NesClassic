## Checklist for making a release:

### Requirements
 - [ ] GitHub account with access to [solidity](https://github.com/ethereum/solidity), [solc-js](https://github.com/ethereum/solc-js),
       [solc-bin](https://github.com/ethereum/solc-bin), [homebrew-ethereum](https://github.com/ethereum/homebrew-ethereum),
       [solidity-blog](https://github.com/ethereum/solidity-blog) and [solidity-portal](https://github.com/ethereum/solidity-portal) repositories.
 - [ ] DockerHub account with push rights to the [``solc`` image](https://hub.docker.com/r/ethereum/solc).
 - [ ] Lauchpad (Ubuntu One) account with a membership in the ["Ethereum" team](https://launchpad.net/~ethereum) and
       a gnupg key for your email in the ``ethereum.org`` domain (has to be version 1, gpg2 won't work).
 - [ ] [npm Registry](https://www.npmjs.com) account added as a collaborator for the [``solc`` package](https://www.npmjs.com/package/solc).
 - [ ] Access to the [solidity_lang Twitter account](https://twitter.com/solidity_lang).
 - [ ] [Reddit](https://www.reddit.com) account that is at least 10 days old with a minimum of 20 comment karma (``/r/ethereum`` requirements).

### Blog Post
 - [ ] Create a post on [solidity-blog](https://github.com/ethereum/solidity-blog) in the ``Releases`` category and explain some of the new features or concepts.
 - [ ] Create a post on [solidity-blog](https://github.com/ethereum/solidity-blog) in the ``Security Alerts`` category in case of important bug(s).

### Documentation check
 - [ ] Run ``make linkcheck`` from within ``docs/`` and fix any broken links it finds. Ignore false positives caused by ``href`` anchors and dummy links not meant to work.

### Changelog
 - [ ] Make sure that all merged PRs that should have changelog entries do have them.
 - [ ] Sort the changelog entries alphabetically and correct any errors you notice. Commit it.
 - [ ] Update the changelog to include a release date.
 - [ ] Run ``scripts/update_bugs_by_version.py`` to regenerate ``bugs_by_version.json`` from the changelog and ``bugs.json``.
       Make sure that the resulting ``bugs_by_version.json`` has a new, empty entry for the new version.
 - [ ] Commit changes, create a pull request and wait for the tests. Then merge it.
 - [ ] Copy the changelog into the release blog post.

### Create the Release
 - [ ] Create a [release on GitHub](https://github.com/ethereum/solidity/releases/new).
       Set the target to the ``develop`` branch and the tag to the new version, e.g. ``v0.8.5``.
       Include the following warning: ``**The release is still in progress and the binaries may not yet be available from all sources.**``.
       Don't publish it yet - click the ``Save draft`` button instead.
 - [ ] Thank voluntary contributors in the GitHub release notes (use ``git shortlog --summary --email v0.5.3..origin/develop``).
 - [ ] Check that all tests on the latest commit in ``develop`` are green.
 - [ ] Click the ``Publish release`` button on the release page, creating the tag.
 - [ ] Wait for the CI runs on the tag itself.

### Upload Release Artifacts and Publish Binaries
 - [ ] Switch to the tag that archives have to be created for.
 - [ ] Create the ``prerelease.txt`` file: (``echo -n > prerelease.txt``).
 - [ ] Run ``scripts/create_source_tarball.sh`` while being on the tag to create the source tarball. This will create the tarball in a directory called ``upload``.
 - [ ] Take the tarball from the upload directory (its name should be ``solidity_x.x.x.tar.gz``, otherwise ``prerelease.txt`` was missing in the step before) and upload the source tarball to the release page.
 - [ ] Take the ``github-binaries.tar`` tarball from ``c_release_binaries`` run of the tagged commit in circle-ci and add all binaries from it to the release page.
       Make sure it contains four binaries: ``solc-windows.exe``, ``solc-macos``, ``solc-static-linux`` and ``soljson.js``.
 - [ ] Take the ``solc-bin-binaries.tar`` tarball from ``c_release_binaries`` run of the tagged commit in circle-ci and add all binaries from it to solc-bin.
 - [ ] Run ``npm run update -- --reuse-hashes`` in ``solc-bin`` and verify that the script has updated ``list.js``, ``list.txt`` and ``list.json`` files correctly and that symlinks to the new release have been added in ``solc-bin/wasm/`` and ``solc-bin/emscripten-wasm32/``.
 - [ ] Create a pull request in solc-bin and merge.

### Homebrew and MacOS
 - [ ] Update the version and the hash (``sha256sum solidity_$VERSION.tar.gz``) in the [``solidity`` formula in Homebrew core repository](https://github.com/Homebrew/homebrew-core/blob/master/Formula/solidity.rb).
 - [ ] Update the version and the hash (``sha256sum solidity_$VERSION.tar.gz``) in [our custom ``solidity`` Homebrew formula](https://github.com/ethereum/homebrew-ethereum/blob/master/solidity.rb).

### Docker
 - [ ] Run ``./scripts/docker_deploy_manual.sh v$VERSION``.

### PPA
 - [ ] Create ``.release_ppa_auth`` at the root of your local Solidity checkout and set ``LAUNCHPAD_EMAIL`` and ``LAUNCHPAD_KEYID`` to your key's email and key id.
 - [ ] Double-check that the ``DISTRIBUTIONS`` list in ``scripts/release_ppa.sh`` and ``scripts/deps-ppa/static-z3.sh`` contains the most recent versions of Ubuntu.
 - [ ] Make sure the [``~ethereum/cpp-build-deps`` PPA repository](https://launchpad.net/~ethereum/+archive/ubuntu/cpp-build-deps) contains ``libz3-static-dev builds`` for all current versions of Ubuntu.
       If not, run ``scripts/deps-ppa/static-z3.sh`` and wait for the builds to succeed before continuing.
 - [ ] Run ``scripts/release_ppa.sh v$VERSION`` to create the PPA release.
 - [ ] Wait for the [``~ethereum/ethereum-static`` PPA](https://launchpad.net/~ethereum/+archive/ubuntu/ethereum-static) build to be finished and published for *all platforms*.
       **SERIOUSLY: DO NOT PROCEED EARLIER!!!**
       *After* the static builds are *published*, copy the static package to the [``~ethereum/ethereum`` PPA](https://launchpad.net/~ethereum/+archive/ubuntu/ethereum)
       for the destination series ``Trusty``, ``Xenial`` and ``Bionic`` while selecting ``Copy existing binaries``.

### Release solc-js
 - [ ] Wait until solc-bin was properly deployed. You can test this via remix - a test run through remix is advisable anyway.
 - [ ] Increment the version number, create a pull request for that, merge it after tests succeeded.
 - [ ] Run ``npm run build:tarball`` in the updated ``solc-js`` repository to create ``solc-<version>.tgz``. Inspect the tarball to ensure that it contains an up to date compiler binary.
 - [ ] Run ``npm run publish:tarball`` to publish the newly created tarball.
 - [ ] Create a tag using ``git tag --annotate v$VERSION`` and push it with ``git push --tags``.

### Post-release
 - [ ] Make sure the documentation for the new release has been published successfully.
       Go to the [documentation status page at ReadTheDocs](https://readthedocs.org/projects/solidity/) and verify that the new version is listed, works and is marked as default.
 - [ ] Remove "still in progress" warning from the release notes.
 - [ ] Publish the blog posts.
 - [ ] Create a commit to increase the version number on ``develop`` in ``CMakeLists.txt`` and add a new skeleton changelog entry.
 - [ ] Announce on Twitter, including links to the release and the blog post.
       Use ``#xp`` at the end of the tweet to automatically cross post the announcement to Fosstodon.
 - [ ] Share the announcement on Reddit in [``/r/ethdev``](https://reddit.com/r/ethdev/), cross-posted to [``/r/ethereum``](https://reddit.com/r/ethereum/).
 - [ ] Share the announcement the [Solidity forum](https://forum.soliditylang.org) in the ``Announcements`` category.
 - [ ] Update the release information section on [soliditylang.org](https://github.com/ethereum/solidity-portal).
 - [ ] Lean back, wait for bug reports and repeat from step 1 :).
