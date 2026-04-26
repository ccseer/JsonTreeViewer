#pragma once

#include <QDebug>

// Common enums and types used across the application

enum class FileMode { Small = 0, Medium = 1, Large = 2, Extreme = 3 };

// Logging helper
#define qprint qDebug()
#define qprint_err qDebug() << "ERROR:" << __FILE__ << __LINE__
