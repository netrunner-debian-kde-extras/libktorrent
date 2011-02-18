/***************************************************************************
 *   Copyright (C) 2005 by Joris Guisson                                   *
 *   joris.guisson@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/
#ifndef BTMOVEDATAFILESJOB_H
#define BTMOVEDATAFILESJOB_H

#include <torrent/job.h>

namespace bt
{
	class TorrentFileInterface;

	/**
	 * @author Joris Guisson <joris.guisson@gmail.com>
	 * KIO::Job to move all the files of a torrent.
	*/
	class MoveDataFilesJob : public Job
	{
		Q_OBJECT
	public:
		MoveDataFilesJob();
		
		/**
			Constructor with a file map.
			@param fmap Map of files and their destinations
		*/
		MoveDataFilesJob(const QMap<TorrentFileInterface*,QString> & fmap);
		virtual ~MoveDataFilesJob();
		
		/**
		 * Add a move to the todo list.
		 * @param src File to move
		 * @param dst Where to move it to
		 */
		void addMove(const QString & src,const QString & dst);
		
		virtual void start();
		virtual void kill(bool quietly = true);
		
		/// Get the file map (could be empty)
		const QMap<TorrentFileInterface*,QString> & fileMap() const {return file_map;}
		
	private slots:
		void onJobDone(KJob* j);
		void onCanceled(KJob* j);
		void onRecoveryJobDone(KJob* j);
		void onTransferred(KJob *job, KJob::Unit unit, qulonglong amount);
		void onSpeed(KJob* job, unsigned long speed);
		
	private:
		void recover(bool delete_active);
		void startMoving();

	private:
		bool err;
		KIO::Job* active_job;
		QString active_src,active_dst;
		QMap<QString,QString> todo;
		QMap<QString,QString> success;
		int running_recovery_jobs;	
		QMap<TorrentFileInterface*,QString> file_map;
		
		bt::Uint64 bytes_moved;
		bt::Uint64 total_bytes;
		bt::Uint64 bytes_moved_current_file;
	};

}

#endif
