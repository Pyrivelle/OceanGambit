#include "LocalSettings.h"

#include <QFile>

void loadLocalSettings() {
    QFile file(qEnvironmentVariable("OCEAN_CONFIG", "C:/CProj/Veylaxis/.env"));
    if (!file.open(QIODevice::ReadOnly)) return;
    while (!file.atEnd()) {
        const auto line = file.readLine().trimmed();
        const int equals = line.indexOf('=');
        if (equals < 0) continue;
        const auto key = line.left(equals).trimmed();
        if (key != "DMU_READER_OPTIONS" && key != "TRITON_LICENSE_USER" &&
            key != "TRITON_LICENSE_CODE") continue;
        if (!qgetenv(key.constData()).isEmpty()) continue;
        auto value = line.mid(equals + 1).trimmed();
        if (value.size() >= 2 &&
            ((value.startsWith('"') && value.endsWith('"')) ||
             (value.startsWith('\'') && value.endsWith('\''))))
            value = value.mid(1, value.size() - 2);
        qputenv(key.constData(), value);
    }
}
