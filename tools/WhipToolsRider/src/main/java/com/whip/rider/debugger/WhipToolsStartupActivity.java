package com.whip.rider.debugger;

import com.intellij.openapi.diagnostic.Logger;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.startup.StartupActivity;

public final class WhipToolsStartupActivity implements StartupActivity.DumbAware {
	private static final Logger Log = Logger.getInstance(WhipToolsStartupActivity.class);

	@Override
	public void runActivity(Project project) {
		Log.info("Whip Tools Rider plugin loaded for project: " + project.getName());
	}
}
