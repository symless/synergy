function config(_github, context) {
  if (!context) throw new Error("Arg `context` not defined.");

  const workflowRun = context.payload.workflow_run;
  const trigger = {
    isWorkflowDispatch: context.eventName === "workflow_dispatch",
    workflowRun: workflowRun && {
      isPush: workflowRun.event === "push",
      isMaster: workflowRun.head_branch.startsWith("master"),
    },
  };

  console.log("Trigger:", trigger);

  const output = {
    // Always run if workflow dispatch, so we can test the workflow manually on CI.
    // Only run on push (i.e. not schedule) to prevent the last PR getting repeated comments.
    runMergeCommentJob:
      trigger.isWorkflowDispatch || (trigger.workflowRun?.isPush && trigger.workflowRun?.isMaster),
  };

  console.log("Config:", output);
  return output;
}

module.exports = {
  config,
};
