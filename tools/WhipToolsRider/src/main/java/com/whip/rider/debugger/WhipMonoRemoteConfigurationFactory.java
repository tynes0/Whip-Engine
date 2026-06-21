package com.whip.rider.debugger;

import com.intellij.execution.configurations.ConfigurationFactory;
import com.intellij.execution.configurations.ConfigurationType;
import com.intellij.execution.configurations.RunConfiguration;
import com.intellij.openapi.project.Project;

public final class WhipMonoRemoteConfigurationFactory extends ConfigurationFactory {
	static final String Id = "WhipMonoRemoteDebugFactory";

	WhipMonoRemoteConfigurationFactory(ConfigurationType type) {
		super(type);
	}

	@Override
	public RunConfiguration createTemplateConfiguration(Project project) {
		return new WhipMonoRemoteConfiguration(project, this, "Whip Mono Debugger");
	}

	@Override
	public String getId() {
		return Id;
	}
}
