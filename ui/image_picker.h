#include <gtk/gtk.h>
#include "deck_plugin.h"

#ifndef __IMAGE_PICKER_H__

#define __IMAGE_PICKER_H__

G_BEGIN_DECLS

#define IMAGE_TYPE_PICKER (image_picker_get_type())
#define IMAGE_PICKER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGE_TYPE_PICKER, ImagePicker))
#define IMAGE_PICKER_CLASS(klass)                                                                  \
    (G_TYPE_CHECK_CLASS_CAST((klass), IMAGE_TYPE_PICKER, ImagePickerClass))
#define IMAGE_IS_PICKER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGE_TYPE_PICKER))
#define IMAGE_IS_PICKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), IMAGE_TYPE_PICKER))
#define IMAGE_PICKER_GET_CLASS(obj)                                                                \
    (G_TYPE_INSTANCE_GET_CLASS((obj), IMAGE_TYPE_PICKER, ImagePickerClass))

typedef struct _ImagePicker ImagePicker;
typedef struct _ImagePickerClass ImagePickerClass;
typedef struct _ImagePickerPrivate ImagePickerPrivate;

struct _ImagePicker {
    /*< private >*/
    GtkBox parent;

    ImagePickerPrivate *priv;
};

struct _ImagePickerClass {
    GtkBoxClass parent_class;
};

GtkWidget *image_picker_new(DeckPlugin *plugin);

G_END_DECLS

#endif /* __IMAGE_PICKER_H__ */