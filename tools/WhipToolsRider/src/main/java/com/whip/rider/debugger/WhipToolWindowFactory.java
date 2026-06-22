package com.whip.rider.debugger;

import com.intellij.openapi.project.DumbAware;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.ui.Messages;
import com.intellij.openapi.wm.ToolWindow;
import com.intellij.openapi.wm.ToolWindowFactory;
import com.intellij.ui.components.JBLabel;
import com.intellij.ui.components.JBScrollPane;
import com.intellij.ui.content.Content;
import com.intellij.ui.content.ContentFactory;

import javax.swing.JButton;
import javax.swing.JPanel;
import javax.swing.JTextArea;
import javax.swing.SwingUtilities;
import java.awt.BorderLayout;
import java.awt.FlowLayout;
import java.util.Optional;

public final class WhipToolWindowFactory implements ToolWindowFactory, DumbAware {
	@Override
	public void createToolWindowContent(Project project, ToolWindow toolWindow) {
		WhipToolWindowPanel panel = new WhipToolWindowPanel(project);
		Content content = ContentFactory.getInstance().createContent(panel, "Debugger", false);
		toolWindow.getContentManager().addContent(content);
		panel.refresh();
	}

	private static final class WhipToolWindowPanel extends JPanel {
		private final Project project;
		private final JTextArea statusText = new JTextArea();
		private final JButton attachButton = new JButton("Attach");
		private final JButton copyButton = new JButton("Copy Endpoint");
		private final JButton refreshButton = new JButton("Refresh");
		private Optional<WhipDebuggerContract> currentContract = Optional.empty();

		private WhipToolWindowPanel(Project project) {
			super(new BorderLayout(8, 8));
			this.project = project;
			statusText.setEditable(false);
			statusText.setLineWrap(true);
			statusText.setWrapStyleWord(true);
			statusText.setOpaque(false);

			JPanel header = new JPanel(new BorderLayout(8, 8));
			header.add(new JBLabel("Whip Debugger"), BorderLayout.WEST);

			JPanel buttons = new JPanel(new FlowLayout(FlowLayout.LEFT, 6, 0));
			buttons.add(attachButton);
			buttons.add(copyButton);
			buttons.add(refreshButton);

			add(header, BorderLayout.NORTH);
			add(new JBScrollPane(statusText), BorderLayout.CENTER);
			add(buttons, BorderLayout.SOUTH);

			attachButton.addActionListener(event -> WhipDebuggerActions.attach(project));
			copyButton.addActionListener(event -> copyEndpoint());
			refreshButton.addActionListener(event -> refresh());
		}

		private void refresh() {
			currentContract = WhipDebuggerActions.findContract(project);
			attachButton.setEnabled(true);
			copyButton.setEnabled(currentContract.isPresent());
			statusText.setText(currentContract.map(this::makeStatusText).orElse(makeMissingStatusText()));
			statusText.setCaretPosition(0);
		}

		private void copyEndpoint() {
			if (currentContract.isEmpty()) {
				Messages.showWarningDialog(project, "No .whip-debugger.json contract was found yet.", "Whip Debugger");
				return;
			}

			WhipDebuggerActions.copyEndpoint(currentContract.get());
			Messages.showInfoMessage(project, "Copied endpoint: " + currentContract.get().getEndpoint(), "Whip Debugger");
		}

		private String makeStatusText(WhipDebuggerContract debugger) {
			StringBuilder text = new StringBuilder();
			text.append("Contract found.\n\n");
			text.append("Enabled: ").append(debugger.isEnabled() ? "true" : "false").append('\n');
			text.append("Endpoint: ").append(debugger.getEndpoint()).append('\n');
			text.append("Connection: checked by Rider when Attach starts\n");
			text.append("Suspend on start: ").append(debugger.isSuspendOnStart() ? "true" : "false").append('\n');
			if (!debugger.getSolution().isBlank()) {
				text.append("Solution: ").append(debugger.getSolution()).append('\n');
			}
			if (!debugger.getScriptModule().isBlank()) {
				text.append("Script module: ").append(debugger.getScriptModule()).append('\n');
			}
			text.append("Contract: ").append(debugger.getContractPath()).append('\n');
			text.append("Log file: ").append(debugger.getLogFile()).append('\n');
			return text.toString();
		}

		private static String makeMissingStatusText() {
			return "No .whip-debugger.json contract found.\n\n" +
				"Open the generated Whip C# solution under Assets/Scripts, or build scripts once from Whip Editor so the debugger contract is generated.";
		}
	}
}
