async function prMergeComment({github, context, version, sha, runId, runName, runResult, repoUrl}) {

  if (!github) {
    throw new Error('GitHub not defined.');
  }

  if (!context) {
    throw new Error('Context not defined.');
  }

  if (!version) {
    console.log('No version found, skipping.');
    return;
  }

  console.log(`Version: ${version}`);

  if (!sha) {
    console.log('No Git SHA found, skipping.');
    return;
  }

  console.log(`SHA: ${sha}`);

  const { data: pullRequests } = await github.rest.repos.listPullRequestsAssociatedWithCommit({
    owner: context.repo.owner,
    repo: context.repo.repo,
    commit_sha: sha,
  });

  if (pullRequests.length < 1) {
    console.log('No PR found, skipping.');
    return;
  }

  console.log(`Found ${pullRequests.length} PR(s).`);
  const prNumber = pullRequests[0].number;

  let body = 'Merge build complete.\n' + `Version: \`${version}\``;

  if (runId) {
    console.log(`Appending result and URL for run ID: ${runId}`);
    const runUrl = `${repoUrl}/actions/runs/${runId}`;
    body += `\nRun: [${runName}](${runUrl}) (${runResult})`;
  } else {
    console.log('No run ID found, skipping run result and URL.');
  }

  console.log(`Commenting on first PR: ${prNumber}`);
  await github.rest.issues.createComment({
    owner: context.repo.owner,
    repo: context.repo.repo,
    issue_number: prNumber,
    body,
  });
}

module.exports = prMergeComment;
