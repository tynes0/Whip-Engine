package com.whip.rider.debugger;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Optional;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class WhipDebuggerContract {
	private static final String STRING_PATTERN_TEMPLATE = "\"%s\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\"";
	private static final String NUMBER_PATTERN_TEMPLATE = "\"%s\"\\s*:\\s*(-?\\d+)";
	private static final String BOOLEAN_PATTERN_TEMPLATE = "\"%s\"\\s*:\\s*(true|false)";

	private final Path contractPath;
	private final boolean enabled;
	private final String host;
	private final int port;
	private final boolean suspendOnStart;
	private final String solution;
	private final String scriptModule;
	private final String logFile;

	private WhipDebuggerContract(
		Path contractPath,
		boolean enabled,
		String host,
		int port,
		boolean suspendOnStart,
		String solution,
		String scriptModule,
		String logFile) {
		this.contractPath = contractPath;
		this.enabled = enabled;
		this.host = host;
		this.port = port;
		this.suspendOnStart = suspendOnStart;
		this.solution = solution;
		this.scriptModule = scriptModule;
		this.logFile = logFile;
	}

	static Optional<WhipDebuggerContract> read(Path contractPath) {
		if (contractPath == null || !Files.isRegularFile(contractPath)) {
			return Optional.empty();
		}

		try {
			String json = Files.readString(contractPath, StandardCharsets.UTF_8);
			String host = readString(json, "host", "127.0.0.1");
			int port = clampPort(readInt(json, "port", 2550));

			return Optional.of(new WhipDebuggerContract(
				contractPath,
				readBoolean(json, "enabled", false),
				host.isBlank() ? "127.0.0.1" : host,
				port,
				readBoolean(json, "suspendOnStart", false),
				readString(json, "solution", ""),
				readString(json, "scriptModule", ""),
				readString(json, "logFile", "MonoDebugger.log")));
		} catch (IOException ignored) {
			return Optional.empty();
		}
	}

	Path getContractPath() {
		return contractPath;
	}

	boolean isEnabled() {
		return enabled;
	}

	String getHost() {
		return host;
	}

	int getPort() {
		return port;
	}

	boolean isSuspendOnStart() {
		return suspendOnStart;
	}

	String getSolution() {
		return solution;
	}

	String getScriptModule() {
		return scriptModule;
	}

	String getLogFile() {
		return logFile;
	}

	String getEndpoint() {
		return host + ":" + port;
	}

	boolean canConnect(Duration timeout) {
		try (Socket socket = new Socket()) {
			socket.connect(new InetSocketAddress(host, port), Math.toIntExact(timeout.toMillis()));
			return true;
		} catch (IOException | ArithmeticException ignored) {
			return false;
		}
	}

	private static String readString(String json, String key, String fallback) {
		Matcher matcher = Pattern.compile(String.format(STRING_PATTERN_TEMPLATE, Pattern.quote(key))).matcher(json);
		return matcher.find() ? unescape(matcher.group(1)) : fallback;
	}

	private static int readInt(String json, String key, int fallback) {
		Matcher matcher = Pattern.compile(String.format(NUMBER_PATTERN_TEMPLATE, Pattern.quote(key))).matcher(json);
		if (!matcher.find()) {
			return fallback;
		}

		try {
			return Integer.parseInt(matcher.group(1));
		} catch (NumberFormatException ignored) {
			return fallback;
		}
	}

	private static boolean readBoolean(String json, String key, boolean fallback) {
		Matcher matcher = Pattern.compile(String.format(BOOLEAN_PATTERN_TEMPLATE, Pattern.quote(key)), Pattern.CASE_INSENSITIVE).matcher(json);
		return matcher.find() ? Boolean.parseBoolean(matcher.group(1)) : fallback;
	}

	private static int clampPort(int port) {
		if (port < 1) {
			return 1;
		}
		if (port > 65535) {
			return 65535;
		}
		return port;
	}

	private static String unescape(String value) {
		StringBuilder result = new StringBuilder(value.length());
		boolean escaping = false;
		for (int i = 0; i < value.length(); ++i) {
			char character = value.charAt(i);
			if (!escaping) {
				if (character == '\\') {
					escaping = true;
				} else {
					result.append(character);
				}
				continue;
			}

			switch (character) {
				case 'n' -> result.append('\n');
				case 'r' -> result.append('\r');
				case 't' -> result.append('\t');
				case '"' -> result.append('"');
				case '\\' -> result.append('\\');
				default -> result.append(character);
			}
			escaping = false;
		}

		if (escaping) {
			result.append('\\');
		}
		return result.toString();
	}
}
