#include "app/ModuleWidget.hpp"
#include "engine/Engine.hpp"
#include "system.hpp"
#include "asset.hpp"
#include "app/Scene.hpp"
#include "app/SVGPanel.hpp"
#include "helpers.hpp"
#include "app.hpp"
#include "settings.hpp"
#include "history.hpp"

#include "osdialog.h"


namespace rack {


struct ModuleDisconnectItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModuleDisconnectItem() {
		text = "Disconnect cables";
		rightText = WINDOW_MOD_CTRL_NAME "+U";
	}
	void onAction(const event::Action &e) override {
		moduleWidget->disconnect();
	}
};

struct ModuleResetItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModuleResetItem() {
		text = "Initialize";
		rightText = WINDOW_MOD_CTRL_NAME "+I";
	}
	void onAction(const event::Action &e) override {
		moduleWidget->reset();
	}
};

struct ModuleRandomizeItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModuleRandomizeItem() {
		text = "Randomize";
		rightText = WINDOW_MOD_CTRL_NAME "+R";
	}
	void onAction(const event::Action &e) override {
		moduleWidget->randomize();
	}
};

struct ModuleCopyItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModuleCopyItem() {
		text = "Copy preset";
		rightText = WINDOW_MOD_CTRL_NAME "+C";
	}
	void onAction(const event::Action &e) override {
		moduleWidget->copyClipboard();
	}
};

struct ModulePasteItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModulePasteItem() {
		text = "Paste preset";
		rightText = WINDOW_MOD_CTRL_NAME "+V";
	}
	void onAction(const event::Action &e) override {
		moduleWidget->pasteClipboard();
	}
};

struct ModuleSaveItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModuleSaveItem() {
		text = "Save preset as";
	}
	void onAction(const event::Action &e) override {
		moduleWidget->saveDialog();
	}
};

struct ModuleLoadItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModuleLoadItem() {
		text = "Load preset";
	}
	void onAction(const event::Action &e) override {
		moduleWidget->loadDialog();
	}
};

struct ModuleCloneItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModuleCloneItem() {
		text = "Duplicate";
		rightText = WINDOW_MOD_CTRL_NAME "+D";
	}
	void onAction(const event::Action &e) override {
		moduleWidget->cloneAction();
	}
};

struct ModuleBypassItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModuleBypassItem() {
		text = "Bypass";
	}
	void step() override {
		rightText = WINDOW_MOD_CTRL_NAME "+E";
		if (!moduleWidget->module)
			return;
		if (moduleWidget->module->bypass)
			rightText = CHECKMARK_STRING " " + rightText;
	}
	void onAction(const event::Action &e) override {
		moduleWidget->bypassAction();
	}
};

struct ModuleDeleteItem : MenuItem {
	ModuleWidget *moduleWidget;
	ModuleDeleteItem() {
		text = "Delete";
		rightText = "Backspace/Delete";
	}
	void onAction(const event::Action &e) override {
		moduleWidget->removeAction();
	}
};


ModuleWidget::~ModuleWidget() {
	setModule(NULL);
}

void ModuleWidget::draw(NVGcontext *vg) {
	if (module && module->bypass) {
		nvgGlobalAlpha(vg, 0.5);
	}
	// nvgScissor(vg, 0, 0, box.size.x, box.size.y);
	Widget::draw(vg);

	// Power meter
	if (module && settings::powerMeter) {
		nvgBeginPath(vg);
		nvgRect(vg,
			0, box.size.y - 20,
			65, 20);
		nvgFillColor(vg, nvgRGBAf(0, 0, 0, 0.75));
		nvgFill(vg);

		std::string cpuText = string::f("%.2f μs", module->cpuTime * 1e6f);
		bndLabel(vg, 2.0, box.size.y - 20.0, INFINITY, INFINITY, -1, cpuText.c_str());

		float p = math::clamp(module->cpuTime / app()->engine->getSampleTime(), 0.f, 1.f);
		nvgBeginPath(vg);
		nvgRect(vg,
			0, (1.f - p) * box.size.y,
			5, p * box.size.y);
		nvgFillColor(vg, nvgRGBAf(1, 0, 0, 1.0));
		nvgFill(vg);
	}

	// nvgResetScissor(vg);
}

void ModuleWidget::drawShadow(NVGcontext *vg) {
	nvgBeginPath(vg);
	float r = 20; // Blur radius
	float c = 20; // Corner radius
	math::Vec b = math::Vec(-10, 30); // Offset from each corner
	nvgRect(vg, b.x - r, b.y - r, box.size.x - 2*b.x + 2*r, box.size.y - 2*b.y + 2*r);
	NVGcolor shadowColor = nvgRGBAf(0, 0, 0, 0.2);
	NVGcolor transparentColor = nvgRGBAf(0, 0, 0, 0);
	nvgFillPaint(vg, nvgBoxGradient(vg, b.x, b.y, box.size.x - 2*b.x, box.size.y - 2*b.y, c, r, shadowColor, transparentColor));
	nvgFill(vg);
}

void ModuleWidget::onHover(const event::Hover &e) {
	OpaqueWidget::onHover(e);

	// Instead of checking key-down events, delete the module even if key-repeat hasn't fired yet and the cursor is hovering over the widget.
	if ((glfwGetKey(app()->window->win, GLFW_KEY_DELETE) == GLFW_PRESS
		|| glfwGetKey(app()->window->win, GLFW_KEY_BACKSPACE) == GLFW_PRESS)
		&& (app()->window->getMods() & WINDOW_MOD_MASK) == 0) {
		removeAction();
		e.consume(NULL);
		return;
	}
}

void ModuleWidget::onButton(const event::Button &e) {
	OpaqueWidget::onButton(e);

	if (e.getConsumed() == this) {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			createContextMenu();
		}
	}
}

void ModuleWidget::onHoverKey(const event::HoverKey &e) {
	if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
		switch (e.key) {
			case GLFW_KEY_I: {
				if ((e.mods & WINDOW_MOD_MASK) == WINDOW_MOD_CTRL) {
					reset();
					e.consume(this);
				}
			} break;
			case GLFW_KEY_R: {
				if ((e.mods & WINDOW_MOD_MASK) == WINDOW_MOD_CTRL) {
					randomize();
					e.consume(this);
				}
			} break;
			case GLFW_KEY_C: {
				if ((e.mods & WINDOW_MOD_MASK) == WINDOW_MOD_CTRL) {
					copyClipboard();
					e.consume(this);
				}
			} break;
			case GLFW_KEY_V: {
				if ((e.mods & WINDOW_MOD_MASK) == WINDOW_MOD_CTRL) {
					pasteClipboard();
					e.consume(this);
				}
			} break;
			case GLFW_KEY_D: {
				if ((e.mods & WINDOW_MOD_MASK) == WINDOW_MOD_CTRL) {
					cloneAction();
				}
			} break;
			case GLFW_KEY_U: {
				if ((e.mods & WINDOW_MOD_MASK) == WINDOW_MOD_CTRL) {
					disconnect();
					e.consume(this);
				}
			} break;
			case GLFW_KEY_E: {
				if ((e.mods & WINDOW_MOD_MASK) == WINDOW_MOD_CTRL) {
					bypassAction();
					e.consume(this);
				}
			} break;
		}
	}

	if (!e.getConsumed())
		OpaqueWidget::onHoverKey(e);
}

void ModuleWidget::onDragStart(const event::DragStart &e) {
	oldPos = box.pos;
	dragPos = app()->scene->rackWidget->lastMousePos.minus(box.pos);
}

void ModuleWidget::onDragEnd(const event::DragEnd &e) {
	if (!box.pos.isEqual(oldPos)) {
		// Push ModuleMove history action
		history::ModuleMove *h = new history::ModuleMove;
		h->moduleId = module->id;
		h->oldPos = oldPos;
		h->newPos = box.pos;
		app()->history->push(h);
	}
}

void ModuleWidget::onDragMove(const event::DragMove &e) {
	if (!settings::lockModules) {
		math::Rect newBox = box;
		newBox.pos = app()->scene->rackWidget->lastMousePos.minus(dragPos);
		app()->scene->rackWidget->requestModuleBoxNearest(this, newBox);
	}
}

void ModuleWidget::setModule(Module *module) {
	if (this->module) {
		delete this->module;
	}
	this->module = module;
}

void ModuleWidget::addInput(PortWidget *input) {
	assert(input->type == PortWidget::INPUT);
	inputs.push_back(input);
	addChild(input);
}

void ModuleWidget::addOutput(PortWidget *output) {
	assert(output->type == PortWidget::OUTPUT);
	outputs.push_back(output);
	addChild(output);
}

void ModuleWidget::addParam(ParamWidget *param) {
	params.push_back(param);
	addChild(param);
}

void ModuleWidget::setPanel(std::shared_ptr<SVG> svg) {
	// Remove old panel
	if (panel) {
		removeChild(panel);
		delete panel;
		panel = NULL;
	}

	{
		SVGPanel *panel = new SVGPanel;
		panel->setBackground(svg);
		addChild(panel);
		box.size = panel->box.size;
		this->panel = panel;
	}
}


json_t *ModuleWidget::toJson() {
	json_t *rootJ = json_object();

	// plugin
	json_object_set_new(rootJ, "plugin", json_string(model->plugin->slug.c_str()));
	// version of plugin
	if (!model->plugin->version.empty())
		json_object_set_new(rootJ, "version", json_string(model->plugin->version.c_str()));
	// model
	json_object_set_new(rootJ, "model", json_string(model->slug.c_str()));

	// Other properties
	if (module) {
		json_t *moduleJ = module->toJson();
		// Merge with rootJ
		json_object_update(rootJ, moduleJ);
		json_decref(moduleJ);
	}
	return rootJ;
}

void ModuleWidget::fromJson(json_t *rootJ) {
	// Check if plugin and model are incorrect
	json_t *pluginJ = json_object_get(rootJ, "plugin");
	std::string pluginSlug;
	if (pluginJ) {
		pluginSlug = json_string_value(pluginJ);
		if (pluginSlug != model->plugin->slug) {
			WARN("Plugin %s does not match ModuleWidget's plugin %s.", pluginSlug.c_str(), model->plugin->slug.c_str());
			return;
		}
	}

	json_t *modelJ = json_object_get(rootJ, "model");
	std::string modelSlug;
	if (modelJ) {
		modelSlug = json_string_value(modelJ);
		if (modelSlug != model->slug) {
			WARN("Model %s does not match ModuleWidget's model %s.", modelSlug.c_str(), model->slug.c_str());
			return;
		}
	}

	// Check plugin version
	json_t *versionJ = json_object_get(rootJ, "version");
	if (versionJ) {
		std::string version = json_string_value(versionJ);
		if (version != model->plugin->version) {
			INFO("Patch created with %s version %s, using version %s.", pluginSlug.c_str(), version.c_str(), model->plugin->version.c_str());
		}
	}

	if (module) {
		module->fromJson(rootJ);
	}
}

void ModuleWidget::copyClipboard() {
	json_t *moduleJ = toJson();
	DEFER({
		json_decref(moduleJ);
	});
	char *moduleJson = json_dumps(moduleJ, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
	DEFER({
		free(moduleJson);
	});
	glfwSetClipboardString(app()->window->win, moduleJson);
}

void ModuleWidget::pasteClipboard() {
	const char *moduleJson = glfwGetClipboardString(app()->window->win);
	if (!moduleJson) {
		WARN("Could not get text from clipboard.");
		return;
	}

	json_error_t error;
	json_t *moduleJ = json_loads(moduleJson, 0, &error);
	if (!moduleJ) {
		WARN("JSON parsing error at %s %d:%d %s", error.source, error.line, error.column, error.text);
		return;
	}
	DEFER({
		json_decref(moduleJ);
	});

	fromJson(moduleJ);
}

void ModuleWidget::load(std::string filename) {
	INFO("Loading preset %s", filename.c_str());

	FILE *file = fopen(filename.c_str(), "r");
	if (!file) {
		WARN("Could not load patch file %s", filename.c_str());
		return;
	}
	DEFER({
		fclose(file);
	});

	json_error_t error;
	json_t *moduleJ = json_loadf(file, 0, &error);
	if (!moduleJ) {
		std::string message = string::f("File is not a valid patch file. JSON parsing error at %s %d:%d %s", error.source, error.line, error.column, error.text);
		osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, message.c_str());
		return;
	}
	DEFER({
		json_decref(moduleJ);
	});

	fromJson(moduleJ);
}

void ModuleWidget::save(std::string filename) {
	INFO("Saving preset %s", filename.c_str());

	json_t *moduleJ = toJson();
	assert(moduleJ);
	DEFER({
		json_decref(moduleJ);
	});

	FILE *file = fopen(filename.c_str(), "w");
	if (!file) {
		WARN("Could not write to patch file %s", filename.c_str());
	}
	DEFER({
		fclose(file);
	});

	json_dumpf(moduleJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
}

void ModuleWidget::loadDialog() {
	std::string dir = asset::user("presets");
	system::createDirectory(dir);

	osdialog_filters *filters = osdialog_filters_parse(PRESET_FILTERS.c_str());
	DEFER({
		osdialog_filters_free(filters);
	});

	char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
	if (!path) {
		// No path selected
		return;
	}
	DEFER({
		free(path);
	});

	load(path);
}

void ModuleWidget::saveDialog() {
	std::string dir = asset::user("presets");
	system::createDirectory(dir);

	osdialog_filters *filters = osdialog_filters_parse(PRESET_FILTERS.c_str());
	DEFER({
		osdialog_filters_free(filters);
	});

	char *path = osdialog_file(OSDIALOG_SAVE, dir.c_str(), "Untitled.vcvm", filters);
	if (!path) {
		// No path selected
		return;
	}
	DEFER({
		free(path);
	});

	std::string pathStr = path;
	std::string extension = string::extension(pathStr);
	if (extension.empty()) {
		pathStr += ".vcvm";
	}

	save(pathStr);
}

void ModuleWidget::disconnect() {
	for (PortWidget *input : inputs) {
		app()->scene->rackWidget->cableContainer->removeAllCables(input);
	}
	for (PortWidget *output : outputs) {
		app()->scene->rackWidget->cableContainer->removeAllCables(output);
	}
}

void ModuleWidget::reset() {
	if (module) {
		app()->engine->resetModule(module);
	}
}

void ModuleWidget::randomize() {
	if (module) {
		app()->engine->randomizeModule(module);
	}
}

void ModuleWidget::removeAction() {
	history::ComplexAction *complexAction = new history::ComplexAction;

	// Push ModuleRemove history action
	history::ModuleRemove *moduleRemove = new history::ModuleRemove;
	moduleRemove->model = model;
	moduleRemove->moduleId = module->id;
	moduleRemove->pos = box.pos;
	moduleRemove->moduleJ = toJson();
	complexAction->push(moduleRemove);

	app()->history->push(complexAction);

	app()->scene->rackWidget->removeModule(this);
	delete this;
}

void ModuleWidget::bypassAction() {
	// Push ModuleBypass history action
	history::ModuleBypass *h = new history::ModuleBypass;
	h->bypass = !module->bypass;
	h->moduleId = module->id;
	app()->history->push(h);
	h->redo();
}

void ModuleWidget::cloneAction() {
	ModuleWidget *clonedModuleWidget = model->createModuleWidget();
	assert(clonedModuleWidget);
	// JSON serialization is the obvious way to do this
	json_t *moduleJ = toJson();
	clonedModuleWidget->fromJson(moduleJ);
	json_decref(moduleJ);

	app()->scene->rackWidget->addModuleAtMouse(clonedModuleWidget);

	// Push ModuleAdd history action
	history::ModuleAdd *h = new history::ModuleAdd;
	h->model = clonedModuleWidget->model;
	h->moduleId = clonedModuleWidget->module->id;
	h->pos = clonedModuleWidget->box.pos;
	app()->history->push(h);
}

void ModuleWidget::createContextMenu() {
	Menu *menu = createMenu();
	assert(model);

	MenuLabel *menuLabel = new MenuLabel;
	menuLabel->text = model->plugin->name + " " + model->name + " " + model->plugin->version;
	menu->addChild(menuLabel);

	ModuleResetItem *resetItem = new ModuleResetItem;
	resetItem->moduleWidget = this;
	menu->addChild(resetItem);

	ModuleRandomizeItem *randomizeItem = new ModuleRandomizeItem;
	randomizeItem->moduleWidget = this;
	menu->addChild(randomizeItem);

	ModuleDisconnectItem *disconnectItem = new ModuleDisconnectItem;
	disconnectItem->moduleWidget = this;
	menu->addChild(disconnectItem);

	ModuleCloneItem *cloneItem = new ModuleCloneItem;
	cloneItem->moduleWidget = this;
	menu->addChild(cloneItem);

	ModuleCopyItem *copyItem = new ModuleCopyItem;
	copyItem->moduleWidget = this;
	menu->addChild(copyItem);

	ModulePasteItem *pasteItem = new ModulePasteItem;
	pasteItem->moduleWidget = this;
	menu->addChild(pasteItem);

	ModuleLoadItem *loadItem = new ModuleLoadItem;
	loadItem->moduleWidget = this;
	menu->addChild(loadItem);

	ModuleSaveItem *saveItem = new ModuleSaveItem;
	saveItem->moduleWidget = this;
	menu->addChild(saveItem);

	ModuleBypassItem *bypassItem = new ModuleBypassItem;
	bypassItem->moduleWidget = this;
	menu->addChild(bypassItem);

	ModuleDeleteItem *deleteItem = new ModuleDeleteItem;
	deleteItem->moduleWidget = this;
	menu->addChild(deleteItem);

	appendContextMenu(menu);
}


} // namespace rack