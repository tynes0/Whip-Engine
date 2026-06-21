plugins {
	id("java")
	id("org.jetbrains.intellij.platform") version "2.16.0"
}

group = "com.whip"
version = "0.1.0"

java {
	toolchain {
		languageVersion.set(JavaLanguageVersion.of(17))
	}
}

dependencies {
	intellijPlatform {
		create(
			providers.gradleProperty("platformType"),
			providers.gradleProperty("platformVersion")
		)
		instrumentationTools()
	}
}

intellijPlatform {
	pluginConfiguration {
		id = "com.whip.rider.tools"
		name = "Whip Tools Rider"
		version = project.version.toString()

		vendor {
			name = "Whip Engine"
		}

		ideaVersion {
			sinceBuild = "243"
		}
	}
}
