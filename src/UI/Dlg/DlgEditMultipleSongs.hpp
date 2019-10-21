#ifndef DLGEDITMULTIPLESONGS_HPP
#define DLGEDITMULTIPLESONGS_HPP





#include <memory>
#include <QDialog>





// fwd:
class ComponentCollection;
class Song;
namespace Ui
{
	class DlgEditMultipleSongs;
}





class DlgEditMultipleSongs:
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	explicit DlgEditMultipleSongs(
		ComponentCollection & aComponents,
		std::vector<std::shared_ptr<Song>> && aSongs,
		QWidget * aParent = nullptr
	);

	virtual ~DlgEditMultipleSongs() override;


private:

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The songs for which the edits are being done. */
	std::vector<std::shared_ptr<Song>> mSongs;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgEditMultipleSongs> mUI;


public slots:

	/** Sets the checked changed to each song, then closes the dialog. */
	void applyChangesAndClose();

	/** The MPM lineedit's text has been edited by the user.
	If the text is an invalid number, colors the line edit with red background, otherwise clears the background. */
	void mpmTextEdited(const QString & aNewText);
};





#endif // DLGEDITMULTIPLESONGS_HPP
