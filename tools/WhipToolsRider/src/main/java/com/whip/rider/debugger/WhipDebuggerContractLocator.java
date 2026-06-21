package com.whip.rider.debugger;

import com.intellij.openapi.project.Project;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Optional;
import java.util.stream.Stream;

final class WhipDebuggerContractLocator {
	private static final String ContractFilename = ".whip-debugger.json";

	private WhipDebuggerContractLocator() {
	}

	static Optional<WhipDebuggerContract> find(Project project) {
		if (project == null || project.getBasePath() == null) {
			return Optional.empty();
		}

		Path basePath = Path.of(project.getBasePath());
		for (Path candidate : preferredCandidates(basePath)) {
			Optional<WhipDebuggerContract> contract = WhipDebuggerContract.read(candidate);
			if (contract.isPresent()) {
				return contract;
			}
		}

		return searchBelow(basePath, 6);
	}

	private static List<Path> preferredCandidates(Path basePath) {
		return List.of(
			basePath.resolve(ContractFilename),
			basePath.resolve("Assets").resolve("Scripts").resolve(ContractFilename),
			basePath.getParent() == null ? basePath.resolve(ContractFilename) : basePath.getParent().resolve(ContractFilename)
		);
	}

	private static Optional<WhipDebuggerContract> searchBelow(Path basePath, int maxDepth) {
		try (Stream<Path> files = Files.walk(basePath, maxDepth)) {
			return files
				.filter(Files::isRegularFile)
				.filter(path -> ContractFilename.equals(path.getFileName().toString()))
				.filter(path -> !isIgnored(path))
				.findFirst()
				.flatMap(WhipDebuggerContract::read);
		} catch (IOException ignored) {
			return Optional.empty();
		}
	}

	private static boolean isIgnored(Path path) {
		for (Path segment : path) {
			String name = segment.toString().toLowerCase();
			if (name.equals("bin") || name.equals("build") || name.equals("obj") || name.equals(".git") || name.equals(".idea")) {
				return true;
			}
		}
		return false;
	}
}
