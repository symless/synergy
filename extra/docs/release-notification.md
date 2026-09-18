# Telling the website about a release

How a published release reaches the website's download page, and why the workflow makes that call
rather than a person. Read this before changing the `website` job in `.github/workflows/ci.yml`,
`extra/scripts/create-website-release.sh`, or the package file names in
`extra/deploy/PackageFileName.cmake`.

Shipped under [S1-2237](https://symless.atlassian.net/browse/S1-2237). The website half is
[WEB-2992](https://symless.atlassian.net/browse/WEB-2992), and its contract, its lifecycle and the
decisions behind them are in `docs/design/release-automation.md` in the website repo; this doc
covers only the call and what has to be true for it to be made. Dragon made the same call first,
under [SD-154](https://symless.atlassian.net/browse/SD-154), and this follows its shape.

Before this, publishing a release built, signed and uploaded the packages and stopped there.
Somebody then had to create the release record and its download rows by hand, and paste in notes
they had written, so making a release was a job in two places and the second one was easy to forget.

## The call

The workflow posts the product's file code, the version, the changes the release was built from,
and the installers the platform jobs built:

```
POST https://symless.com/synergy/api/releases
Authorization: Bearer <WEBSITE_API_TOKEN>

{
  "fileCode": "synergy-personal-v1",
  "version": "1.21.2",
  "changes": ["...", "..."],
  "packages": [{"fileName": "synergy_1.21.2_windows_x64.msi", "os": "windows-10", "arch": "X64"}]
}
```

The website creates the release and its downloads, writes customer-facing notes from the changes,
copies the packages out of the archive bucket, and holds the release for a person to approve. So
this call never publishes anything: publishing the GitHub release and pressing approve are the two
manual acts, and everything between them is the workflow's.

## Where the changes come from

The changes are the issues carrying this version as their fix version, each sent as its summary
followed by its description. Issue summaries here are already written for a customer and describe
the solution rather than the code, and the description says what the change is for, which is
grounding neither a commit subject nor a diff can give.

Decisions worth keeping:

- **Jira first, commits as the fallback.** When no issue carries the version, the commit subjects
  since the previous tag are sent instead. A release whose notes came from commit subjects is worth
  more than a release held up by an unticked field, and the fallback prints why it happened.
- **A failed Jira read stops the release call.** Silently falling back on an auth or network error
  would quietly produce worse notes with no sign anything went wrong; only genuinely finding no
  issues falls back.
- **The website stays agnostic.** `changes` is an array of strings, so where they came from is this
  repo's business.

## Decisions

**Two editions, not three.** Personal and Business are the same build, listed twice in the
website's catalog, so one tag here creates a release under both `synergy-personal-v1` and
`synergy-business-v1`. Enterprise is a genuinely different package: `symless/synergy-ee` builds it
with its own package prefix into `synergy1/enterprise`, and telling the website about it is that
repo's job, not this one's. Sending it from here would point an Enterprise release at file names
this repo never produced.

**A release that already exists is not worth failing over.** Because this creates two releases, a
run that got the first one in before something went wrong has to be safe to repeat. A 409 is
reported and passed over so a re-run finishes the rest; anything else the website refuses fails the
job. If the call fails the packages are already published and only the website is behind, so the
recovery is never to re-tag: fix the website end and re-run the job, or, where the fix is in the
script, run the fixed script by hand against the same tag, because a re-run checks out the tag's
own copy of it and fails the same way.

**It is a job of its own, after every platform.** The website creates the download rows for every
platform at once, so a release row that exists before Windows has finished offers files that are
not in the bucket yet. This is the one job in the workflow with no `always()`: `s3-upload`
deliberately ships whatever built, because a flaky distro must not sink a nightly snapshot, but a
release missing a download is worse than one nobody made. `s3-upload` is also a dependency for
ordering rather than only for safety, since the website starts copying the packages out of the
bucket as soon as it is told about them.

**Only a release event.** A manual run can publish to S3 to test the packaging, but it has no tag,
so it has no release to name. It must not reach the review queue either: what lands there is a
release somebody is about to approve, and a test build sitting in it is a test build that
eventually gets approved. Pre-releases do go, because a beta is a release people download.

**The version is cmake's, and the tag is checked against it.** `extra/cmake/Version.cmake` is the
only thing that names a build, and the packages, the archive folder they upload to and the folder
the website copies them into are all named from it, so that is what is sent. The tag is a separate
act that can disagree with it, and a tag that did would point the website at an archive folder that
was never written, so the script refuses a tag that is not `v` plus the version rather than
discovering it later as an empty copy.

**The base URL follows the same override the rest of the product uses.** `SYNERGY_WEBSITE_URL_BASE`
points the script at a local or staging website, so a trial run cannot end up talking to two
different places.

## Which packages the release has

The workflow sends the installers it built, read off the platform jobs' artifacts, so the list can
never drift from what the packaging scripts actually name. Each one is sent with the catalog row it
belongs under, and that catalog has no plain "Linux": it names distributions and their releases.
A package's name says what it was built on, and `package_rows_for` in the script says where it is
listed. Anything built that it cannot name stops the release rather than being left out quietly.

A file can be listed under more than one row, and the rows can differ by edition:

- **Enterprise Linux is one build under two names.** Business customers run Red Hat and the
  personal side runs the free rebuilds, so the same rpm goes under `rhel-8`/`rhel-9` for Business
  and `rocky-8`/`rocky-9` for Personal. It is built on Rocky, and the claim that it serves Red Hat
  rests on the two being ABI-compatible rather than on a test run, because the Enterprise Linux
  legs are the only Linux legs with tests switched off. The package itself is named `el-8`/`el-9`
  after neither of them: a file wearing two labels must not be named for one of them, and naming it
  for the distribution would also move the name on every Rocky point release, since the token comes
  from `/etc/os-release` `VERSION_ID`. The download page's half of this is
  [WEB-3077](https://symless.atlassian.net/browse/WEB-3077); until that lands the rows exist but
  nothing renders them.
- **Raspberry Pi OS takes the Debian arm64 package.** Pi OS is Debian, and the row used to be
  filled by an Ubuntu 22.04 arm64 build that is no longer made, which left the download pointing at
  a file that is not in the bucket. It now takes the Debian 12 arm64 package, the release current
  Pi OS is built on.
- **A slug keeps the dot the file name replaces.** The catalog's slug is `ubuntu-24.04` and
  `macos-12.0`, while `PackageFileName.cmake` writes `ubuntu-24-04` and `mac_x64`, because the
  website's `slugify` preserves `.` and the file name scheme does not. This reads as a typo and is
  not.

The portable Windows `.7z` is skipped. The download page has no button for it, and listing it
against the same row as the `.msi` would hide one of the two behind the other.

## What the repository needs

`WEBSITE_API_TOKEN` is an organization secret holding a website API token, the same token-guarded
path every other machine caller uses, and `JIRA_USER_EMAIL` and `JIRA_API_TOKEN` read the issues
fixed in a version. All three are shared with selected repositories, so this one has to be on each
secret's list. Without the website token the job fails on every release, which is deliberate: a
release the website was never told about should not look like a release that went out. Without the
Jira pair the job still succeeds, writing its notes from commit subjects instead.
