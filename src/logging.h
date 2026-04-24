#pragma once

#include <QDebug>

// Unified logging macros for JsonTreeViewer
// All logs will be prefixed with [JsonTreeViewer] for easy identification in
// Seer logs

#define qprint qDebug() << "[JsonTreeViewer]"

#define qprint_err qprint << "[JsonTreeViewer]" << "Error" << Q_FUNC_INFO
