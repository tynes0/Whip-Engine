package com.whip.rider.debugger;

import com.intellij.execution.configurations.ConfigurationTypeBase;
import com.intellij.openapi.util.IconLoader;

public final class WhipMonoRemoteConfigurationType extends ConfigurationTypeBase {
	static final String Id = "WhipMonoRemoteDebug";

	public WhipMonoRemoteConfigurationType() {
		super(
			Id,
			"Whip Mono Debugger",
			"Attach Rider to Whip's Mono soft debugger endpoint",
			IconLoader.getIcon("/icons/whip.svg", WhipMonoRemoteConfigurationType.class));
		addFactory(new WhipMonoRemoteConfigurationFactory(this));
	}
}
