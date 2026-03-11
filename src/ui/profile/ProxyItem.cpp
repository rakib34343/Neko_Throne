#include "include/ui/profile/ProxyItem.h"

#include <QFontMetrics>
#include <QMessageBox>

ProxyItem::ProxyItem(QWidget *parent, const std::shared_ptr<Configs::ProxyEntity> &ent, QListWidgetItem *item)
    : QWidget(parent), ui(new Ui::ProxyItem) {
    ui->setupUi(this);
    this->setLayoutDirection(Qt::LeftToRight);

    this->item = item;
    this->ent = ent;
    if (ent == nullptr) return;

    refresh_data();
}

ProxyItem::~ProxyItem() {
    delete ui;
}

void ProxyItem::refresh_data() {
    ui->type->setText(ent->outbound->DisplayType());
    const int nameW = ui->name->width();
    auto fmName = ui->name->fontMetrics();
    ui->name->setText(nameW > 4
        ? fmName.elidedText(ent->outbound->DisplayName(), Qt::ElideRight, nameW - 4)
        : ent->outbound->DisplayName());
    const int addrW = ui->address->width();
    auto fmAddr = ui->address->fontMetrics();
    ui->address->setText(addrW > 4
        ? fmAddr.elidedText(ent->outbound->DisplayAddress(), Qt::ElideRight, addrW - 4)
        : ent->outbound->DisplayAddress());
    ui->traffic->setText(ent->traffic_data->DisplayTraffic());
    ui->test_result->setText(ent->DisplayTestResult());

    runOnThread(
        [=,this] {
            adjustSize();
            item->setSizeHint(sizeHint());
            dynamic_cast<QWidget *>(parent())->adjustSize();
        },
        this);
}

void ProxyItem::on_remove_clicked() {
    if (!this->remove_confirm ||
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(ent->outbound->DisplayName())) == QMessageBox::StandardButton::Yes) {
        // TODO do remove (or not) -> callback
        delete item;
    }
}

QPushButton *ProxyItem::get_change_button() {
    return ui->change;
}
