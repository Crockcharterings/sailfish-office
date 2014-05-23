/*
 *
 */

#include "pdfdocument.h"
#include "pdfrenderthread.h"
#include "pdfjob.h"

#include <QDebug>
#include <QUrl>

#include <poppler-qt5.h>

class PDFDocument::Private
{
public:
    Private() : document( nullptr ), completed(false) { }

    PDFRenderThread* thread;

    Poppler::Document* document;
    QString source;
    bool completed;
};

PDFDocument::PDFDocument(QObject* parent)
    : QObject(parent), d(new Private())
{
    d->thread = new PDFRenderThread{ this };
    connect( d->thread, &PDFRenderThread::loadFinished, this, &PDFDocument::documentLoaded );
    connect( d->thread, &PDFRenderThread::loadFinished, this, &PDFDocument::pageCountChanged );
    connect( d->thread, &PDFRenderThread::jobFinished, this, &PDFDocument::jobFinished );
}

PDFDocument::~PDFDocument()
{
    delete d->thread;
}

QString PDFDocument::source() const
{
    return d->source;
}

int PDFDocument::pageCount() const
{
    if(d->thread && d->thread->isLoaded())
    {
        return d->thread->pageCount();
    }

    return 0;
}

QObject* PDFDocument::tocModel() const
{
    return d->thread->tocModel();
}

bool PDFDocument::isLoaded() const
{
    return d->thread->isLoaded();
}

PDFDocument::LinkMap PDFDocument::linkTargets() const
{
    return d->thread->linkTargets();
}

void PDFDocument::classBegin()
{

}

void PDFDocument::componentComplete()
{
    if(!d->source.isEmpty())
    {
        LoadDocumentJob* job = new LoadDocumentJob{ QUrl{d->source}.toLocalFile() };
        d->thread->queueJob( job );
    }

    d->completed = true;
}

void PDFDocument::setSource(const QString& source)
{
    if (d->source != source)
    {
        d->source = source;
        if(source.startsWith("/"))
            d->source.prepend("file://");

        if(d->completed) {
            LoadDocumentJob* job = new LoadDocumentJob{ QUrl{source}.toLocalFile() };
            d->thread->queueJob( job );
        }

        emit sourceChanged();
    }
}

void PDFDocument::requestPage(int index, int size, QQuickWindow *window )
{
    if(!isLoaded())
        return;

    RenderPageJob* job = new RenderPageJob{ index, size, window };
    d->thread->queueJob( job );
}

void PDFDocument::prioritizeRequest(int index, int size)
{
    if (!isLoaded())
        return;
    d->thread->prioritizeJob(index, size);
}

void PDFDocument::cancelPageRequest(int index)
{
    if (!isLoaded())
        return;
    d->thread->cancelRenderJob(index);
}

void PDFDocument::requestPageSizes()
{
    if(!isLoaded())
        return;

    PageSizesJob* job = new PageSizesJob{};
    d->thread->queueJob( job );
}

void PDFDocument::jobFinished(PDFJob* job)
{
    switch(job->type()) {
        case PDFJob::RenderPageJob: {
            RenderPageJob* j = static_cast<RenderPageJob*>(job);
            emit pageFinished(j->m_index, j->m_page);
            break;
        }
        case PDFJob::PageSizesJob: {
            PageSizesJob* j = static_cast<PageSizesJob*>(job);
            emit pageSizesFinished(j->m_pageSizes);
            break;
        }
        default:
            break;
    }

    job->deleteLater();
}
