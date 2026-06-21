package com.whip.rider.debugger;

import com.intellij.openapi.actionSystem.ActionUpdateThread;
import com.intellij.openapi.actionSystem.AnActionEvent;
import com.intellij.openapi.project.DumbAwareAction;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.ui.Messages;

public final class AttachToWhipDebuggerAction extends DumbAwareAction {
	@Override
	public void update(AnActionEvent event) {
		event.getPresentation().setVisible(true);
		event.getPresentation().setEnabled(true);
	}

	@Override
	public ActionUpdateThread getActionUpdateThread() {
		return ActionUpdateThread.BGT;
	}

	@Override
	public void actionPerformed(AnActionEvent event) {
		Project project = event.getProject();
		if (project == null) {
			Messages.showInfoMessage(
				"Open the generated Whip C# solution in Rider, then run this action again.",
				"Whip Debugger");
			return;
		}

		WhipDebuggerActions.attach(project);
	}
}
