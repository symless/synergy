function config(github, _context, _core) {
  if (!github) {
    throw new Error("GitHub not defined.");
  }

  const workflowRun = github.event.workflow_run;
  const trigger = {
    isWorkflowDispatch: github.event_name === "workflow_dispatch",
    workflowRun: {
      isPush: workflowRun.event === "push",
      isMaster: workflowRun.head_branch.startsWith("master"),
    },
  };

  console.log("Trigger:", trigger);

  const output = {
    // Always run if workflow dispatch, so we can test the workflow manually on CI.
    // Only run on push (i.e. not schedule) to prevent the last PR getting repeated comments.
    runMergeCommentJob:
      trigger.isWorkflowDispatch || (trigger.workflowRun.isPush && trigger.workflowRun.isMaster),
  };

  console.log("Config:", output);
  return output;
}

module.exports = {
  config,
};
