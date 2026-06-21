plugins {
	id("java")
	id("org.jetbrains.intellij.platform")
}

group = "com.whip"
version = "0.1.1"

val whipBuildRoot = providers.gradleProperty("whipBuildRoot")
val localRiderPath = providers.gradleProperty("localRiderPath")

whipBuildRoot.orNull?.let {
	layout.buildDirectory.set(file(it))
}

java {
	toolchain {
		languageVersion.set(JavaLanguageVersion.of(17))
	}
}

dependencies {
	intellijPlatform {
		if (localRiderPath.isPresent) {
			local(localRiderPath)
		} else {
			create(
				providers.gradleProperty("platformType"),
				providers.gradleProperty("platformVersion")
			) {
				useInstaller.set(false)
			}
		}
	}
}

intellijPlatform {
	buildSearchableOptions.set(false)
	instrumentCode.set(false)
	sandboxContainer.set(layout.buildDirectory.dir("idea-sandbox"))

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

tasks.named("buildSearchableOptions") {
	enabled = false
}
