#include <QDebug>

#include "extractionworker.h"
#include "pyinstarchive.h"

ExtractionWorker::ExtractionWorker(const QString& archivePath, const QString& outputDir, const QString& selectedFile, QObject* parent)
		: QObject(parent), m_archivePath(archivePath), m_outputDir(outputDir), m_selectedFile(selectedFile) {
}

void ExtractionWorker::startExtraction()
{
		try {
				PyInstArchive archive(m_archivePath.toStdString());
				if (!archive.open()) {
						emit errorOccurred("Failed to open the archive.");
						return;
				}
				if (!archive.checkFile()) {
						emit errorOccurred("Invalid file format.");
						return;
				}
				if (!archive.getCArchiveInfo()) {
						emit errorOccurred("Failed to get archive info.");
						return;
				}

				archive.parseTOC();
				const std::vector<CTOCEntry>& entries = archive.getTOCList();
				std::mutex fileMutex;
				std::mutex printMutex;

				std::vector<CTOCEntry> toExtract;
				if (!m_selectedFile.isEmpty()) {
						for (const auto& entry : entries) {
								if (QString::fromStdString(entry.getName()) == m_selectedFile) {
										toExtract.push_back(entry);
										break;
								}
						}
				}
				else {
						toExtract = entries;
				}

				int total = toExtract.size();
				for (int i = 0; i < total; ++i) {
						archive.decompressAndExtractFile(toExtract[i], m_outputDir.toStdString(), fileMutex, printMutex);
						emit progress((i + 1) * 100 / total);
				}

				emit finished();
		}
		catch (const std::exception& e) {
				emit errorOccurred(QString("Exception during extraction: %1").arg(e.what()));
		}
}

