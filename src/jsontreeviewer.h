#pragma once

#include "seer/viewerbase.h"

class QLineEdit;
class JsonTreeView;
class QPushButton;

class JsonTreeViewer : public ViewerBase {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ViewerBase_iid FILE "JsonTreeViewer.json")
    Q_INTERFACES(ViewerBase)
public:
    explicit JsonTreeViewer(QWidget* parent = nullptr);
    ~JsonTreeViewer() override;

    QString name() const override
    {
        return "JsonTreeViewer";
    }

    QSize getContentSize() const override;
    void updateDPR(qreal) override;

private:
    void loadImpl(QBoxLayout* lay_content, QHBoxLayout* lay_ctrlbar) override;
    void onTextViewBtnClicked();

    void initTopWnd();
    struct {
        QWidget* wnd_bg   = nullptr;
        QPushButton* btn  = nullptr;
        QLineEdit* filter = nullptr;
    } m_top;

    QPushButton* m_btn_text_view;

    JsonTreeView* m_view;
};
