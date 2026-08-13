/****************************************************************************
** Meta object code from reading C++ file 'playback_api_client.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../playback_api_client.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'playback_api_client.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN17PlaybackApiClientE_t {};
} // unnamed namespace

template <> constexpr inline auto PlaybackApiClient::qt_create_metaobjectdata<qt_meta_tag_ZN17PlaybackApiClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "PlaybackApiClient",
        "loginSucceeded",
        "",
        "expires_in_seconds",
        "timelineReceived",
        "PlaybackTimeline",
        "timeline",
        "thumbnailReceived",
        "channel_id",
        "utc_ms",
        "QImage",
        "image",
        "playbackSessionCreated",
        "PlaybackSession",
        "session",
        "playbackSessionDeleted",
        "session_id",
        "requestFailed",
        "operation",
        "message",
        "http_status"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'loginSucceeded'
        QtMocHelpers::SignalData<void(qint64)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 3 },
        }}),
        // Signal 'timelineReceived'
        QtMocHelpers::SignalData<void(const PlaybackTimeline &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'thumbnailReceived'
        QtMocHelpers::SignalData<void(int, qint64, const QImage &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::LongLong, 9 }, { 0x80000000 | 10, 11 },
        }}),
        // Signal 'playbackSessionCreated'
        QtMocHelpers::SignalData<void(const PlaybackSession &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Signal 'playbackSessionDeleted'
        QtMocHelpers::SignalData<void(const QString &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 },
        }}),
        // Signal 'requestFailed'
        QtMocHelpers::SignalData<void(const QString &, const QString &, int)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 18 }, { QMetaType::QString, 19 }, { QMetaType::Int, 20 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PlaybackApiClient, qt_meta_tag_ZN17PlaybackApiClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject PlaybackApiClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17PlaybackApiClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17PlaybackApiClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17PlaybackApiClientE_t>.metaTypes,
    nullptr
} };

void PlaybackApiClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PlaybackApiClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->loginSucceeded((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 1: _t->timelineReceived((*reinterpret_cast<std::add_pointer_t<PlaybackTimeline>>(_a[1]))); break;
        case 2: _t->thumbnailReceived((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QImage>>(_a[3]))); break;
        case 3: _t->playbackSessionCreated((*reinterpret_cast<std::add_pointer_t<PlaybackSession>>(_a[1]))); break;
        case 4: _t->playbackSessionDeleted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->requestFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< PlaybackTimeline >(); break;
            }
            break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< PlaybackSession >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (PlaybackApiClient::*)(qint64 )>(_a, &PlaybackApiClient::loginSucceeded, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaybackApiClient::*)(const PlaybackTimeline & )>(_a, &PlaybackApiClient::timelineReceived, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaybackApiClient::*)(int , qint64 , const QImage & )>(_a, &PlaybackApiClient::thumbnailReceived, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaybackApiClient::*)(const PlaybackSession & )>(_a, &PlaybackApiClient::playbackSessionCreated, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaybackApiClient::*)(const QString & )>(_a, &PlaybackApiClient::playbackSessionDeleted, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (PlaybackApiClient::*)(const QString & , const QString & , int )>(_a, &PlaybackApiClient::requestFailed, 5))
            return;
    }
}

const QMetaObject *PlaybackApiClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PlaybackApiClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17PlaybackApiClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int PlaybackApiClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void PlaybackApiClient::loginSucceeded(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void PlaybackApiClient::timelineReceived(const PlaybackTimeline & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void PlaybackApiClient::thumbnailReceived(int _t1, qint64 _t2, const QImage & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}

// SIGNAL 3
void PlaybackApiClient::playbackSessionCreated(const PlaybackSession & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void PlaybackApiClient::playbackSessionDeleted(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void PlaybackApiClient::requestFailed(const QString & _t1, const QString & _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
