package com.whip.rider.debugger;

import com.intellij.openapi.actionSystem.ActionManager;
import com.intellij.openapi.actionSystem.ActionPlaces;
import com.intellij.openapi.actionSystem.AnAction;
import com.intellij.openapi.ide.CopyPasteManager;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.ui.Messages;

import java.awt.datatransfer.StringSelection;
import java.time.Duration;
import java.util.Optional;

final class WhipDebuggerActions {
	private static final Duration ConnectionProbeTimeout = Duration.ofMillis(600);
	private static final String[] KnownAttachActionIds = {
		"RiderAttachToProcess",
		"RiderAttachToProcessAction",
		"RiderAttachToRemoteDebugger",
		"AttachToProcess",
		"AttachToProcessAction"
	};

	private WhipDebuggerActions() {
	}

	static void attach(Project project) {
		Optional<WhipDebuggerContract> contract = WhipDebuggerContractLocator.find(project);
		if (contract.isEmpty()) {
			Messages.showErrorDialog(
				project,
				"Could not find .whip-debugger.json.\n\nOpen the generated C# workspace under Assets/Scripts, or build scripts once from Whip Editor so the contract file is generated.",
				"Whip Debugger");
			return;
		}

		WhipDebuggerContract debugger = contract.get();
		copyEndpoint(debugger);

		if (!debugger.isEnabled()) {
			Messages.showWarningDialog(
				project,
				"Whip script debugging is disabled for this project.\n\nEnable Project Settings > Script Debugger in Whip Editor, save, restart the editor, then run this action again.\n\nEndpoint copied anyway: " + debugger.getEndpoint(),
				"Whip Debugger Disabled");
			return;
		}

		boolean reachable = debugger.canConnect(ConnectionProbeTimeout);
		boolean openedAttachUi = false;
		if (reachable) {
			openedAttachUi = tryOpenAttachUi();
		}

		if (reachable) {
			Messages.showInfoMessage(project, makeReachableMessage(debugger, openedAttachUi), "Whip Debugger Ready");
		} else {
			Messages.showWarningDialog(project, makeUnreachableMessage(debugger), "Whip Debugger Not Reachable");
		}
	}

	static Optional<WhipDebuggerContract> findContract(Project project) {
		return WhipDebuggerContractLocator.find(project);
	}

	static void copyEndpoint(WhipDebuggerContract debugger) {
		CopyPasteManager.getInstance().setContents(new StringSelection(debugger.getEndpoint()));
	}

	private static boolean tryOpenAttachUi() {
		ActionManager actionManager = ActionManager.getInstance();
		for (String actionId : KnownAttachActionIds) {
			AnAction action = actionManager.getAction(actionId);
			if (action == null) {
				continue;
			}

			actionManager.tryToExecute(action, null, null, ActionPlaces.MAIN_MENU, true);
			return true;
		}
		return false;
	}

	private static String makeReachableMessage(WhipDebuggerContract debugger, boolean openedAttachUi) {
		StringBuilder message = new StringBuilder();
		message.append("Whip Mono debugger is listening at ").append(debugger.getEndpoint()).append(".\n\n");
		message.append("The endpoint has been copied to the clipboard.");
		if (openedAttachUi) {
			message.append("\n\nRider's attach UI was opened. Choose the Whip/Mono debugger target if Rider asks for a process or endpoint.");
		} else {
			message.append("\n\nOpen Rider's attach debugger action and use the copied endpoint.");
		}
		appendContractDetails(message, debugger);
		return message.toString();
	}

	private static String makeUnreachableMessage(WhipDebuggerContract debugger) {
		StringBuilder message = new StringBuilder();
		message.append("Could not connect to ").append(debugger.getEndpoint()).append(".\n\n");
		message.append("Make sure Whip Editor is running with the project open and that Project Settings > Script Debugger is enabled.");
		message.append("\n\nThe endpoint has been copied to the clipboard.");
		appendContractDetails(message, debugger);
		return message.toString();
	}

	private static void appendContractDetails(StringBuilder message, WhipDebuggerContract debugger) {
		message.append("\n\nContract: ").append(debugger.getContractPath());
		if (!debugger.getSolution().isBlank()) {
			message.append("\nSolution: ").append(debugger.getSolution());
		}
		if (!debugger.getScriptModule().isBlank()) {
			message.append("\nScript module: ").append(debugger.getScriptModule());
		}
		message.append("\nSuspend on start: ").append(debugger.isSuspendOnStart() ? "true" : "false");
		message.append("\nLog file: ").append(debugger.getLogFile());
	}
}
