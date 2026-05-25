import FileManagerClient from '@/components/files/FileManagerClient';
import { FIRMWARE_VERSION } from '@/content/meta';

export const metadata = {
  title: 'File Manager — UniGeek',
  description: 'Browse, upload, and download files on a connected UniGeek device over USB serial.',
};

export default function FilesPage() {
  return (
    <>
      <header className="page-header">
        <div className="page-header-meta">
          <span>// file · manager · uart</span>
          <span>Website {FIRMWARE_VERSION === 'dev' ? 'dev' : `v${FIRMWARE_VERSION}`}</span>
        </div>
        <h1 className="page-title">File Manager</h1>
        <p className="page-subtitle">
          Manage the on-device storage of a connected UniGeek over USB serial. No WiFi, no password
          &mdash; just plug in the board and click Connect.
        </p>
      </header>

      <FileManagerClient expectedVersion={FIRMWARE_VERSION} />
    </>
  );
}
