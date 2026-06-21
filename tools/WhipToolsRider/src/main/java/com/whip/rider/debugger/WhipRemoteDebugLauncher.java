package com.whip.rider.debugger;

import com.intellij.execution.ProgramRunnerUtil;
import com.intellij.execution.RunManager;
import com.intellij.execution.RunnerAndConfigurationSettings;
import com.intellij.execution.configurations.ConfigurationFactory;
import com.intellij.execution.configurations.ConfigurationType;
import com.intellij.execution.configurations.ConfigurationTypeUtil;
import com.intellij.execution.executors.DefaultDebugExecutor;
import com.intellij.openapi.diagnostic.Logger;
import com.intellij.openapi.project.Project;

final class WhipRemoteDebugLauncher {
	private static final Logger Log = Logger.getInstance(WhipRemoteDebugLauncher.class);
	private static final String ConfigurationName = "Whip Mono Debugger";

	private WhipRemoteDebugLauncher() {
	}

	static boolean start(Project project, WhipDebuggerContract debugger) {
		try {
			ConfigurationType type = ConfigurationTypeUtil.findConfigurationType(WhipMonoRemoteConfigurationType.class);
			ConfigurationFactory factory = type.getConfigurationFactories()[0];
			RunManager runManager = RunManager.getInstance(project);
			RunnerAndConfigurationSettings settings = findOrCreateSettings(runManager, factory);
			WhipMonoRemoteConfiguration configuration = (WhipMonoRemoteConfiguration)settings.getConfiguration();
			configuration.setAddress(debugger.getHost());
			configuration.setPort(debugger.getPort());
			configuration.setListenPortForConnections(false);
			settings.setName(ConfigurationName + " (" + debugger.getEndpoint() + ")");
			runManager.setSelectedConfiguration(settings);
			ProgramRunnerUtil.executeConfiguration(settings, DefaultDebugExecutor.getDebugExecutorInstance());
			return true;
		} catch (RuntimeException exception) {
			Log.warn("Could not start Whip Mono debugger attach directly.", exception);
			return false;
		}
	}

	private static RunnerAndConfigurationSettings findOrCreateSettings(RunManager runManager, ConfigurationFactory factory) {
		for (RunnerAndConfigurationSettings settings : runManager.getAllSettings()) {
			if (settings.getConfiguration() instanceof WhipMonoRemoteConfiguration) {
				return settings;
			}
		}

		RunnerAndConfigurationSettings settings = runManager.createConfiguration(ConfigurationName, factory);
		runManager.addConfiguration(settings);
		return settings;
	}
}
