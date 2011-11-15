/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qvariant.h"

// Gui types
#include "qbitmap.h"
#include "qbrush.h"
#include "qcolor.h"
#include "qcursor.h"
#include "qdatastream.h"
#include "qdebug.h"
#include "qfont.h"
#include "qimage.h"
#include "qkeysequence.h"
#include "qtransform.h"
#include "qmatrix.h"
#include "qpalette.h"
#include "qpen.h"
#include "qpixmap.h"
#include "qpolygon.h"
#include "qregion.h"
#include "qtextformat.h"
#include "qmatrix4x4.h"
#include "qvector2d.h"
#include "qvector3d.h"
#include "qvector4d.h"
#include "qquaternion.h"

// Core types
#include "qvariant.h"
#include "qbitarray.h"
#include "qbytearray.h"
#include "qdatastream.h"
#include "qdebug.h"
#include "qmap.h"
#include "qdatetime.h"
#include "qeasingcurve.h"
#include "qlist.h"
#include "qstring.h"
#include "qstringlist.h"
#include "qurl.h"
#include "qlocale.h"

#ifndef QT_NO_GEOM_VARIANT
#include "qsize.h"
#include "qpoint.h"
#include "qrect.h"
#include "qline.h"
#endif

#include <float.h>

#include "private/qvariant_p.h"
#include <private/qmetatype_p.h>

QT_BEGIN_NAMESPACE

Q_GUI_EXPORT const QVariant::Handler *qt_widgets_variant_handler = 0;

Q_CORE_EXPORT const QVariant::Handler *qcoreVariantHandler();

namespace {
template<typename T>
struct TypeDefiniton {
    static const bool IsAvailable = true;
};
// Ignore these types, as incomplete
#ifdef QT_NO_GEOM_VARIANT
template<> struct TypeDefiniton<QRect> { static const bool IsAvailable = false; };
template<> struct TypeDefiniton<QRectF> { static const bool IsAvailable = false; };
template<> struct TypeDefiniton<QSize> { static const bool IsAvailable = false; };
template<> struct TypeDefiniton<QSizeF> { static const bool IsAvailable = false; };
template<> struct TypeDefiniton<QLine> { static const bool IsAvailable = false; };
template<> struct TypeDefiniton<QLineF> { static const bool IsAvailable = false; };
template<> struct TypeDefiniton<QPoint> { static const bool IsAvailable = false; };
template<> struct TypeDefiniton<QPointF> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_SHORTCUT
template<> struct TypeDefiniton<QKeySequence> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_CURSOR
template<> struct TypeDefiniton<QCursor> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_MATRIX4X4
template<> struct TypeDefiniton<QMatrix4x4> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_VECTOR2D
template<> struct TypeDefiniton<QVector2D> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_VECTOR3D
template<> struct TypeDefiniton<QVector3D> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_VECTOR4D
template<> struct TypeDefiniton<QVector4D> { static const bool IsAvailable = false; };
#endif
#ifdef QT_NO_QUATERNION
template<> struct TypeDefiniton<QQuaternion> { static const bool IsAvailable = false; };
#endif

struct CoreAndGuiTypesFilter {
    template<typename T>
    struct Acceptor {
        static const bool IsAccepted = (QTypeModuleInfo<T>::IsCore || QTypeModuleInfo<T>::IsGui) && TypeDefiniton<T>::IsAvailable;
    };
};
} // namespace

static void construct(QVariant::Private *x, const void *copy)
{
    const int type = x->type;
    QVariantConstructor<CoreAndGuiTypesFilter> constructor(x, copy);
    QMetaTypeSwitcher::switcher<void>(constructor, type, 0);

    // FIXME This is an ugly hack if QVariantConstructor fails to build a value it constructs an invalid type
    if (Q_UNLIKELY(x->type == QVariant::Invalid)) {
        if (type == 62) {
            // small 'trick' to let a QVariant(Qt::blue) create a variant
            // of type QColor
            // TODO Get rid of this hack.
            x->type = QVariant::Color;
            QColor color(*reinterpret_cast<const Qt::GlobalColor *>(copy));
            v_construct<QColor>(x, &color);
            return;
        }
        if (type == QVariant::Icon || type == QVariant::SizePolicy) {
            // TODO we need to clean up variant handlers, so they are replacament, not extension
            x->type = type;
            if (qt_widgets_variant_handler) {
                qt_widgets_variant_handler->construct(x, copy);
            }
        }
    }
}

static void clear(QVariant::Private *d)
{
    const int type = d->type;
    if (type == QVariant::Icon || type == QVariant::SizePolicy) {
        // TODO we need to clean up variant handlers, so they are replacament, not extension
        if (qt_widgets_variant_handler) {
            qt_widgets_variant_handler->clear(d);
            return;
        }
    }
    QVariantDestructor<CoreAndGuiTypesFilter> destructor(d);
    QMetaTypeSwitcher::switcher<void>(destructor, type, 0);
}

// This class is a hack that customizes access to QPolygon
template<class Filter>
class QGuiVariantIsNull : public QVariantIsNull<Filter> {
    typedef QVariantIsNull<Filter> Base;
public:
    QGuiVariantIsNull(const QVariant::Private *d)
        : QVariantIsNull<Filter>(d)
    {}
    template<typename T>
    bool delegate(const T *p) { return Base::delegate(p); }
    bool delegate(const QPolygon*) { return v_cast<QPolygon>(Base::m_d)->isEmpty(); }
    bool delegate(const void *p) { return Base::delegate(p); }
};
static bool isNull(const QVariant::Private *d)
{
    QGuiVariantIsNull<CoreAndGuiTypesFilter> isNull(d);
    return QMetaTypeSwitcher::switcher<bool>(isNull, d->type, 0);
}

// This class is a hack that customizes access to QPixmap, QBitmap and QCursor
template<class Filter>
class QGuiVariantComparator : public QVariantComparator<Filter> {
    typedef QVariantComparator<Filter> Base;
public:
    QGuiVariantComparator(const QVariant::Private *a, const QVariant::Private *b)
        : QVariantComparator<Filter>(a, b)
    {}
    template<typename T>
    bool delegate(const T *p)
    {
        if (Q_UNLIKELY(Base::m_a->type == QVariant::Icon || Base::m_a->type == QVariant::SizePolicy)) {
            // TODO we need to clean up variant handlers, so they are replacament, not extension
            if (Q_LIKELY(qt_widgets_variant_handler))
                return qt_widgets_variant_handler->compare(Base::m_a, Base::m_b);
        }
        return Base::delegate(p);
    }
    bool delegate(const QPixmap*)
    {
        return v_cast<QPixmap>(Base::m_a)->cacheKey() == v_cast<QPixmap>(Base::m_b)->cacheKey();
    }
    bool delegate(const QBitmap*)
    {
        return v_cast<QBitmap>(Base::m_a)->cacheKey() == v_cast<QBitmap>(Base::m_b)->cacheKey();
    }
#ifndef QT_NO_CURSOR
    bool delegate(const QCursor*)
    {
        return v_cast<QCursor>(Base::m_a)->shape() == v_cast<QCursor>(Base::m_b)->shape();
    }
#endif
    bool delegate(const void *p) { return Base::delegate(p); }
};

static bool compare(const QVariant::Private *a, const QVariant::Private *b)
{
    QGuiVariantComparator<CoreAndGuiTypesFilter> comparator(a, b);
    return QMetaTypeSwitcher::switcher<bool>(comparator, a->type, 0);
}

static bool convert(const QVariant::Private *d, QVariant::Type t,
                 void *result, bool *ok)
{
    switch (t) {
    case QVariant::ByteArray:
        if (d->type == QVariant::Color) {
            *static_cast<QByteArray *>(result) = v_cast<QColor>(d)->name().toLatin1();
            return true;
        }
        break;
    case QVariant::String: {
        QString *str = static_cast<QString *>(result);
        switch (d->type) {
#ifndef QT_NO_SHORTCUT
        case QVariant::KeySequence:
            *str = QString(*v_cast<QKeySequence>(d));
            return true;
#endif
        case QVariant::Font:
            *str = v_cast<QFont>(d)->toString();
            return true;
        case QVariant::Color:
            *str = v_cast<QColor>(d)->name();
            return true;
        default:
            break;
        }
        break;
    }
    case QVariant::Pixmap:
        if (d->type == QVariant::Image) {
            *static_cast<QPixmap *>(result) = QPixmap::fromImage(*v_cast<QImage>(d));
            return true;
        } else if (d->type == QVariant::Bitmap) {
            *static_cast<QPixmap *>(result) = *v_cast<QBitmap>(d);
            return true;
        } else if (d->type == QVariant::Brush) {
            if (v_cast<QBrush>(d)->style() == Qt::TexturePattern) {
                *static_cast<QPixmap *>(result) = v_cast<QBrush>(d)->texture();
                return true;
            }
        }
        break;
    case QVariant::Image:
        if (d->type == QVariant::Pixmap) {
            *static_cast<QImage *>(result) = v_cast<QPixmap>(d)->toImage();
            return true;
        } else if (d->type == QVariant::Bitmap) {
            *static_cast<QImage *>(result) = v_cast<QBitmap>(d)->toImage();
            return true;
        }
        break;
    case QVariant::Bitmap:
        if (d->type == QVariant::Pixmap) {
            *static_cast<QBitmap *>(result) = *v_cast<QPixmap>(d);
            return true;
        } else if (d->type == QVariant::Image) {
            *static_cast<QBitmap *>(result) = QBitmap::fromImage(*v_cast<QImage>(d));
            return true;
        }
        break;
#ifndef QT_NO_SHORTCUT
    case QVariant::Int:
        if (d->type == QVariant::KeySequence) {
            *static_cast<int *>(result) = (int)(*(v_cast<QKeySequence>(d)));
            return true;
        }
        break;
#endif
    case QVariant::Font:
        if (d->type == QVariant::String) {
            QFont *f = static_cast<QFont *>(result);
            f->fromString(*v_cast<QString>(d));
            return true;
        }
        break;
    case QVariant::Color:
        if (d->type == QVariant::String) {
            static_cast<QColor *>(result)->setNamedColor(*v_cast<QString>(d));
            return static_cast<QColor *>(result)->isValid();
        } else if (d->type == QVariant::ByteArray) {
            static_cast<QColor *>(result)->setNamedColor(QString::fromLatin1(
                                *v_cast<QByteArray>(d)));
            return true;
        } else if (d->type == QVariant::Brush) {
            if (v_cast<QBrush>(d)->style() == Qt::SolidPattern) {
                *static_cast<QColor *>(result) = v_cast<QBrush>(d)->color();
                return true;
            }
        }
        break;
    case QVariant::Brush:
        if (d->type == QVariant::Color) {
            *static_cast<QBrush *>(result) = QBrush(*v_cast<QColor>(d));
            return true;
        } else if (d->type == QVariant::Pixmap) {
            *static_cast<QBrush *>(result) = QBrush(*v_cast<QPixmap>(d));
            return true;
        }
        break;
#ifndef QT_NO_SHORTCUT
    case QVariant::KeySequence: {
        QKeySequence *seq = static_cast<QKeySequence *>(result);
        switch (d->type) {
        case QVariant::String:
            *seq = QKeySequence(*v_cast<QString>(d));
            return true;
        case QVariant::Int:
            *seq = QKeySequence(d->data.i);
            return true;
        default:
            break;
        }
    }
#endif
    default:
        break;
    }
    return qcoreVariantHandler()->convert(d, t, result, ok);
}

#if !defined(QT_NO_DEBUG_STREAM) && !defined(Q_BROKEN_DEBUG_STREAM)
static void streamDebug(QDebug dbg, const QVariant &v)
{
    switch(v.type()) {
    case QVariant::Cursor:
#ifndef QT_NO_CURSOR
//        dbg.nospace() << qvariant_cast<QCursor>(v); //FIXME
#endif
        break;
    case QVariant::Bitmap:
//        dbg.nospace() << qvariant_cast<QBitmap>(v); //FIXME
        break;
    case QVariant::Polygon:
        dbg.nospace() << qvariant_cast<QPolygon>(v);
        break;
    case QVariant::Region:
        dbg.nospace() << qvariant_cast<QRegion>(v);
        break;
    case QVariant::Font:
//        dbg.nospace() << qvariant_cast<QFont>(v);  //FIXME
        break;
    case QVariant::Matrix:
        dbg.nospace() << qvariant_cast<QMatrix>(v);
        break;
    case QVariant::Transform:
        dbg.nospace() << qvariant_cast<QTransform>(v);
        break;
    case QVariant::Pixmap:
//        dbg.nospace() << qvariant_cast<QPixmap>(v); //FIXME
        break;
    case QVariant::Image:
//        dbg.nospace() << qvariant_cast<QImage>(v); //FIXME
        break;
    case QVariant::Brush:
        dbg.nospace() << qvariant_cast<QBrush>(v);
        break;
    case QVariant::Color:
        dbg.nospace() << qvariant_cast<QColor>(v);
        break;
    case QVariant::Palette:
//        dbg.nospace() << qvariant_cast<QPalette>(v); //FIXME
        break;
#ifndef QT_NO_ICON
    case QVariant::Icon:
//        dbg.nospace() << qvariant_cast<QIcon>(v); // FIXME
        break;
#endif
    case QVariant::SizePolicy:
//        dbg.nospace() << qvariant_cast<QSizePolicy>(v); //FIXME
        break;
#ifndef QT_NO_SHORTCUT
    case QVariant::KeySequence:
        dbg.nospace() << qvariant_cast<QKeySequence>(v);
        break;
#endif
    case QVariant::Pen:
        dbg.nospace() << qvariant_cast<QPen>(v);
        break;
#ifndef QT_NO_MATRIX4X4
    case QVariant::Matrix4x4:
        dbg.nospace() << qvariant_cast<QMatrix4x4>(v);
        break;
#endif
#ifndef QT_NO_VECTOR2D
    case QVariant::Vector2D:
        dbg.nospace() << qvariant_cast<QVector2D>(v);
        break;
#endif
#ifndef QT_NO_VECTOR3D
    case QVariant::Vector3D:
        dbg.nospace() << qvariant_cast<QVector3D>(v);
        break;
#endif
#ifndef QT_NO_VECTOR4D
    case QVariant::Vector4D:
        dbg.nospace() << qvariant_cast<QVector4D>(v);
        break;
#endif
#ifndef QT_NO_QUATERNION
    case QVariant::Quaternion:
        dbg.nospace() << qvariant_cast<QQuaternion>(v);
        break;
#endif
    default:
        qcoreVariantHandler()->debugStream(dbg, v);
        break;
    }
}
#endif

const QVariant::Handler qt_gui_variant_handler = {
    construct,
    clear,
    isNull,
#ifndef QT_NO_DATASTREAM
    0,
    0,
#endif
    compare,
    convert,
    0,
#if !defined(QT_NO_DEBUG_STREAM) && !defined(Q_BROKEN_DEBUG_STREAM)
    streamDebug
#else
    0
#endif
};

struct QMetaTypeGuiHelper
{
    QMetaType::Creator creator;
    QMetaType::Deleter deleter;
#ifndef QT_NO_DATASTREAM
    QMetaType::SaveOperator saveOp;
    QMetaType::LoadOperator loadOp;
#endif
    QMetaType::Constructor constructor;
    QMetaType::Destructor destructor;
    int size;
};

extern Q_CORE_EXPORT const QMetaTypeGuiHelper *qMetaTypeGuiHelper;


#ifdef QT_NO_DATASTREAM
#  define Q_DECL_METATYPE_HELPER(TYPE) \
     typedef void *(*QCreate##TYPE)(const TYPE *); \
     static const QCreate##TYPE qCreate##TYPE = qMetaTypeCreateHelper<TYPE>; \
     typedef void (*QDelete##TYPE)(TYPE *); \
     static const QDelete##TYPE qDelete##TYPE = qMetaTypeDeleteHelper<TYPE>; \
     typedef void *(*QConstruct##TYPE)(void *, const TYPE *); \
     static const QConstruct##TYPE qConstruct##TYPE = qMetaTypeConstructHelper<TYPE>; \
     typedef void (*QDestruct##TYPE)(TYPE *); \
     static const QDestruct##TYPE qDestruct##TYPE = qMetaTypeDestructHelper<TYPE>;
#else
#  define Q_DECL_METATYPE_HELPER(TYPE) \
     typedef void *(*QCreate##TYPE)(const TYPE *); \
     static const QCreate##TYPE qCreate##TYPE = qMetaTypeCreateHelper<TYPE>; \
     typedef void (*QDelete##TYPE)(TYPE *); \
     static const QDelete##TYPE qDelete##TYPE = qMetaTypeDeleteHelper<TYPE>; \
     typedef void *(*QConstruct##TYPE)(void *, const TYPE *); \
     static const QConstruct##TYPE qConstruct##TYPE = qMetaTypeConstructHelper<TYPE>; \
     typedef void (*QDestruct##TYPE)(TYPE *); \
     static const QDestruct##TYPE qDestruct##TYPE = qMetaTypeDestructHelper<TYPE>; \
     typedef void (*QSave##TYPE)(QDataStream &, const TYPE *); \
     static const QSave##TYPE qSave##TYPE = qMetaTypeSaveHelper<TYPE>; \
     typedef void (*QLoad##TYPE)(QDataStream &, TYPE *); \
     static const QLoad##TYPE qLoad##TYPE = qMetaTypeLoadHelper<TYPE>;
#endif

Q_DECL_METATYPE_HELPER(QFont)
Q_DECL_METATYPE_HELPER(QPixmap)
Q_DECL_METATYPE_HELPER(QBrush)
Q_DECL_METATYPE_HELPER(QColor)
Q_DECL_METATYPE_HELPER(QPalette)
Q_DECL_METATYPE_HELPER(QImage)
Q_DECL_METATYPE_HELPER(QPolygon)
Q_DECL_METATYPE_HELPER(QRegion)
Q_DECL_METATYPE_HELPER(QBitmap)
#ifndef QT_NO_CURSOR
Q_DECL_METATYPE_HELPER(QCursor)
#endif
#ifndef QT_NO_SHORTCUT
Q_DECL_METATYPE_HELPER(QKeySequence)
#endif
Q_DECL_METATYPE_HELPER(QPen)
Q_DECL_METATYPE_HELPER(QTextLength)
Q_DECL_METATYPE_HELPER(QTextFormat)
Q_DECL_METATYPE_HELPER(QMatrix)
Q_DECL_METATYPE_HELPER(QTransform)
#ifndef QT_NO_MATRIX4X4
Q_DECL_METATYPE_HELPER(QMatrix4x4)
#endif
#ifndef QT_NO_VECTOR2D
Q_DECL_METATYPE_HELPER(QVector2D)
#endif
#ifndef QT_NO_VECTOR3D
Q_DECL_METATYPE_HELPER(QVector3D)
#endif
#ifndef QT_NO_VECTOR4D
Q_DECL_METATYPE_HELPER(QVector4D)
#endif
#ifndef QT_NO_QUATERNION
Q_DECL_METATYPE_HELPER(QQuaternion)
#endif
Q_DECL_METATYPE_HELPER(QPolygonF)

#ifdef QT_NO_DATASTREAM
#  define Q_IMPL_METATYPE_HELPER(TYPE) \
     { reinterpret_cast<QMetaType::Creator>(qCreate##TYPE), \
       reinterpret_cast<QMetaType::Deleter>(qDelete##TYPE), \
       reinterpret_cast<QMetaType::Constructor>(qConstruct##TYPE), \
       reinterpret_cast<QMetaType::Destructor>(qDestruct##TYPE), \
       sizeof(TYPE) \
     }
#else
#  define Q_IMPL_METATYPE_HELPER(TYPE) \
     { reinterpret_cast<QMetaType::Creator>(qCreate##TYPE), \
       reinterpret_cast<QMetaType::Deleter>(qDelete##TYPE), \
       reinterpret_cast<QMetaType::SaveOperator>(qSave##TYPE), \
       reinterpret_cast<QMetaType::LoadOperator>(qLoad##TYPE), \
       reinterpret_cast<QMetaType::Constructor>(qConstruct##TYPE), \
       reinterpret_cast<QMetaType::Destructor>(qDestruct##TYPE), \
       sizeof(TYPE) \
     }
#endif

static const QMetaTypeGuiHelper qVariantGuiHelper[] = {
    Q_IMPL_METATYPE_HELPER(QFont),
    Q_IMPL_METATYPE_HELPER(QPixmap),
    Q_IMPL_METATYPE_HELPER(QBrush),
    Q_IMPL_METATYPE_HELPER(QColor),
    Q_IMPL_METATYPE_HELPER(QPalette),
    Q_IMPL_METATYPE_HELPER(QImage),
    Q_IMPL_METATYPE_HELPER(QPolygon),
    Q_IMPL_METATYPE_HELPER(QRegion),
    Q_IMPL_METATYPE_HELPER(QBitmap),
#ifdef QT_NO_CURSOR
    {0, 0, 0, 0, 0, 0, 0},
#else
    Q_IMPL_METATYPE_HELPER(QCursor),
#endif
#ifdef QT_NO_SHORTCUT
    {0, 0, 0, 0, 0, 0, 0},
#else
    Q_IMPL_METATYPE_HELPER(QKeySequence),
#endif
    Q_IMPL_METATYPE_HELPER(QPen),
    Q_IMPL_METATYPE_HELPER(QTextLength),
    Q_IMPL_METATYPE_HELPER(QTextFormat),
    Q_IMPL_METATYPE_HELPER(QMatrix),
    Q_IMPL_METATYPE_HELPER(QTransform),
#ifndef QT_NO_MATRIX4X4
    Q_IMPL_METATYPE_HELPER(QMatrix4x4),
#else
    {0, 0, 0, 0, 0, 0, 0},
#endif
#ifndef QT_NO_VECTOR2D
    Q_IMPL_METATYPE_HELPER(QVector2D),
#else
    {0, 0, 0, 0, 0, 0, 0},
#endif
#ifndef QT_NO_VECTOR3D
    Q_IMPL_METATYPE_HELPER(QVector3D),
#else
    {0, 0, 0, 0, 0, 0, 0},
#endif
#ifndef QT_NO_VECTOR4D
    Q_IMPL_METATYPE_HELPER(QVector4D),
#else
    {0, 0, 0, 0, 0, 0, 0},
#endif
#ifndef QT_NO_QUATERNION
    Q_IMPL_METATYPE_HELPER(QQuaternion),
#else
    {0, 0, 0, 0, 0, 0, 0},
#endif
    Q_IMPL_METATYPE_HELPER(QPolygonF)
};

static const QVariant::Handler *qt_guivariant_last_handler = 0;
int qRegisterGuiVariant()
{
    qt_guivariant_last_handler = QVariant::handler;
    QVariant::handler = &qt_gui_variant_handler;
    qMetaTypeGuiHelper = qVariantGuiHelper;
    return 1;
}
Q_CONSTRUCTOR_FUNCTION(qRegisterGuiVariant)

int qUnregisterGuiVariant()
{
    QVariant::handler = qt_guivariant_last_handler;
    qMetaTypeGuiHelper = 0;
    return 1;
}
Q_DESTRUCTOR_FUNCTION(qUnregisterGuiVariant)


QT_END_NAMESPACE
