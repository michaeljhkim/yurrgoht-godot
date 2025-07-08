#include "editor/plugins/editor_plugin.h"

class EditorPlugin_Wrapper : public EditorPlugin {
public:
    using EditorPlugin::get_undo_redo;      // this is a protected function -> expose publicly
};