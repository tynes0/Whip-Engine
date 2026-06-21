package com.whip.rider.debugger;

import com.intellij.execution.Executor;
import com.intellij.execution.configurations.ConfigurationFactory;
import com.intellij.execution.configurations.RunProfileState;
import com.intellij.execution.runners.ExecutionEnvironment;
import com.intellij.openapi.project.Project;
import com.jetbrains.rider.run.configurations.remote.DotNetRemoteConfiguration;
import com.jetbrains.rider.run.configurations.remote.MonoConnectRemoteProfileState;

public final class WhipMonoRemoteConfiguration extends DotNetRemoteConfiguration {
	public WhipMonoRemoteConfiguration(Project project, ConfigurationFactory factory, String name) {
		super(project, factory, name);
		setAddress("127.0.0.1");
		setPort(2550);
		setListenPortForConnections(false);
	}

	@Override
	public RunProfileState getState(Executor executor, ExecutionEnvironment environment) {
		return new MonoConnectRemoteProfileState(this, environment);
	}
}
