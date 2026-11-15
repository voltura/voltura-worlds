const { setTimeout: delay } = require('node:timers/promises');

const requestOptions = { timeout: 15_000 };
const finalFailureStatuses = new Set([
  'deployment_failed',
  'deployment_content_failed',
  'deployment_cancelled',
  'deployment_lost',
]);

async function waitForGitHub(timeout, check) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    try {
      if (await check()) return true;
    } catch (error) {
      const status = error.status;
      if (status !== 404 && status !== 429 && !(status >= 500 && status < 600)) throw error;
    }
    await delay(5_000);
  }
  return false;
}

module.exports = async ({ github, context, core }) => {
  const artifactId = Number(process.env.ARTIFACT_ID);
  if (!Number.isSafeInteger(artifactId) || artifactId <= 0) {
    throw new Error('The Pages upload did not return a valid artifact ID.');
  }

  const ready = await waitForGitHub(180_000, async () => {
    const { data: artifact } = await github.rest.actions.getArtifact({
      ...context.repo,
      artifact_id: artifactId,
      request: requestOptions,
    });
    if (artifact.expired) throw new Error(`Pages artifact ${artifactId} expired.`);
    return artifact.id === artifactId;
  });
  if (!ready) throw new Error(`Pages artifact ${artifactId} did not become available.`);

  const token = await core.getIDToken();
  let deployment;
  try {
    const response = await github.request('POST /repos/{owner}/{repo}/pages/deployments', {
      ...context.repo,
      artifact_id: artifactId,
      pages_build_version: context.sha,
      oidc_token: token,
      request: requestOptions,
    });
    deployment = response.data;
  } catch (error) {
    const requestId = error.response?.headers['x-github-request-id'];
    throw new Error(`GitHub could not deploy Pages artifact ${artifactId} (status ${error.status ?? 'unknown'}, request ${requestId ?? 'unknown'}).`);
  }

  const deploymentParams = {
    ...context.repo,
    pages_deployment_id: deployment.id,
    request: requestOptions,
  };
  core.info(`Deploying Pages artifact ${artifactId} as ${deployment.id}.`);
  const published = await waitForGitHub(600_000, async () => {
    const { data } = await github.request(
      'GET /repos/{owner}/{repo}/pages/deployments/{pages_deployment_id}',
      deploymentParams,
    );
    if (finalFailureStatuses.has(data.status)) {
      throw new Error(`Pages deployment ${deployment.id} ended with ${data.status}.`);
    }
    return data.status === 'succeed';
  });
  if (!published) {
    await github.request(
      'POST /repos/{owner}/{repo}/pages/deployments/{pages_deployment_id}/cancel',
      deploymentParams,
    );
    throw new Error(`Pages deployment ${deployment.id} timed out and was cancelled.`);
  }
  core.setOutput('page_url', deployment.page_url);
};
